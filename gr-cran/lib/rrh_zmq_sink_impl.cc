/* -*- c++ -*- */
/*
 * Copyright 2022 gr-cran author.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "rrh_zmq_sink_impl.h"

namespace gr
{
  namespace cran
  {
    rrh_zmq_sink::sptr
    rrh_zmq_sink::make(std::string broker_addr, std::vector<int> frequency_indices, int samp_rate)
    {
      return gnuradio::get_initial_sptr(new rrh_zmq_sink_impl(broker_addr, frequency_indices, samp_rate));
    }
    /*
     * The private constructor
     */
    rrh_zmq_sink_impl::rrh_zmq_sink_impl(std::string broker_addr, std::vector<int> frequency_indices, int samp_rate)
        : gr::sync_block("rrh_zmq_sink",
                         gr::io_signature::make(1, frequency_indices.size(), sizeof(gr_complex)),
                         gr::io_signature::make(0, 0, 0))
    {
      srandom(time(NULL));
      sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));
      debug_print("frequency_indices.size() " << frequency_indices.size(),m_identity,"rrh_zmq_sink",YELLOW);
      

      m_broker_addr = broker_addr;
      m_frequency_indices = frequency_indices;
      m_samp_rate = samp_rate;
      // m_streaming_channels.resize(m_frequency_indices.size(), INACTIVE);

      // zmq binding
      debug_print("Starting",m_identity,"rrh_zmq_sink",YELLOW);
      m_broker_socket = new zmq::socket_t(m_context, zmq::socket_type::dealer);
      m_broker_socket->connect(m_broker_addr);
      m_broker_socket->set(zmq::sockopt::linger, 0);

      // request a buffer
      debug_print("requesting buffer to "<<m_broker_addr,m_identity,"rrh_zmq_sink",YELLOW);

      s_send(*m_broker_socket, "NEW_BUFFER");

      std::string empty = s_recv(*m_broker_socket);
      assert(empty.size() == 0);

      m_buffer_addr = s_recv(*m_broker_socket);
      debug_print("Buffer listening on " << m_buffer_addr,m_identity,"rrh_zmq_sink",YELLOW);

      // connect to buffer
      m_push_socket.setsockopt(ZMQ_SNDTIMEO, 0);
      m_push_socket.connect(m_buffer_addr);

      debug_print("Ready",m_identity,"rrh_zmq_sink",YELLOW);

      last_heartbeat = std::chrono::steady_clock::now();
      m_frame_list.resize(get_max_channels());
      start_time = std::chrono::steady_clock::now();

      for (int i = 0; i < 10000000; i++)
      {
        m_debug_vect.push_back(gr_complex(i, 0));
      }
    }

    /*
     * Our virtual destructor.
     */
    rrh_zmq_sink_impl::~rrh_zmq_sink_impl()
    {
    }

  void rrh_zmq_sink_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required) 
  {
    ninput_items_required[0] = 4096; // Defines the minimum size of packet we will send with ZMQ
  }

    int rrh_zmq_sink_impl::work(int noutput_items,
                                gr_vector_const_void_star &input_items,
                                gr_vector_void_star &output_items)
    {
      // const gr_complex *in = (const gr_complex *)input_items[0];
      //--------------------------------------------------
      zmq::pollitem_t items[1] = {*m_broker_socket, 0, ZMQ_POLLIN, 0};
      // poll message
      zmq::poll(&items[0], 1, 0);
      if (items[0].revents & ZMQ_POLLIN)
      {
        std::string message = s_recv(*m_broker_socket);
        assert(message.size() == 0);
        // message = s_recv(*m_broker_socket);
        // uint16_t end_of_stream;

        zmq::message_t msg_buff;
        m_broker_socket->recv(msg_buff);
        // gr::cran::Frame_info *frame_info = (gr::cran::Frame_info *)msg_buff.data();
        uint16_t *channel_id = (uint16_t *)msg_buff.data();

        // memcpy(&end_of_stream, message.data(), sizeof(end_of_stream));

        debug_print("Received end of stream for channel_id: " << (int)*channel_id,m_identity,"rrh_zmq_sink",YELLOW);

        auto it = find(m_frequency_indices.begin(), m_frequency_indices.end(), *channel_id);
  
        // If channel found
        if (it != m_frequency_indices.end()) 
        {
          m_frame_list[it-m_frequency_indices.begin()].clear();
        }
        else{
          error_print("Channel index " << *channel_id << " not supported by this RRH shouldn't have received end of streaming request!",m_identity);
        }
        
        // if (itr != m_frequency_indices.end())
        // {
        //   m_streaming_channels[itr - m_frequency_indices.begin()] = INACTIVE;
        // }
        // else
        // {
        //   std::cerr << "Channel index " << frame_info->channel_id << " not supported by this RRH shouldn't have received end of streaming request!" << std::endl;
        // }
      }
      //--------------------------------------------------
      // see if a new frame has been detected by looking for tags
      std::vector<tag_t> tags;
      for (int ch = 0; ch < m_frequency_indices.size(); ch++)
      {
        get_tags_in_window(tags, ch, 0, noutput_items, pmt::string_to_symbol("frame_info"));
        for (int i = 0; i < tags.size(); i++)
        {
          bool accept_new_frame = true;
          // Read frame info
          pmt::pmt_t err = pmt::string_to_symbol("Error when parsing 'frame info' tag.");
          int sf = pmt::to_long(pmt::dict_ref(tags[i].value, pmt::string_to_symbol("sf"), err));
          // get starting time
          int64_t time_full = pmt::to_uint64(pmt::dict_ref(tags[i].value, pmt::string_to_symbol("full_secs"), err));
          double time_frac = pmt::to_double(pmt::dict_ref(tags[i].value, pmt::string_to_symbol("frac_secs"), err));

         
          // search if channel and SF already being streamed
          auto it = std::find_if(m_frame_list[ch].begin(), m_frame_list[ch].end(), [&sf](Frame_info const &frame)
                                 { return frame.sf == sf; });
          while (it != m_frame_list[ch].end())
          {
            // if not enough time after previous detection, ignore
            // debug_print("time since last frame with sf = "<<time_full-it->time_full + time_frac-it->time_frac,m_identity,"rrh_zmq_sink",YELLOW);
            
            if (((time_full-it->time_full) + time_frac-it->time_frac) < float(NEW_FRAME_MIN_DELAY * (1u << sf)) / m_bw)
            {
              //debug_print("Not enough time passed, new frame detection not considered",m_identity,"rrh_zmq_sink",YELLOW);
              accept_new_frame = false;
            }
            it++;
            it = std::find_if(it, m_frame_list[ch].end(), [&sf](Frame_info const &frame)
                                 { return frame.sf == sf; });
          }
          if(accept_new_frame){
            // else
            // {
            //   work_list.push_back(std::pair<uint16_t, std::chrono::steady_clock::time_point>{frame_pre, std::chrono::steady_clock::now() + std::chrono::milliseconds{int(float(NEW_FRAME_MIN_DELAY * (1u << sf)) / (m_bw)*1000)}});
            // }
            // generate frame preamble
            uint8_t ch_id = m_frequency_indices[ch];
            debug_print("new work for channel id:" << (int)ch_id << ", fc: " << center_freq_MHz[m_frequency_indices[ch]] << ", sf" << sf << ", at absolute time " << time_full << " " << time_frac,m_identity,"rrh_zmq_sink",YELLOW);
            
            Frame_info new_frame_info;
            new_frame_info.frame_state = NEW_FRAME;
            new_frame_info.channel_id = ch_id;
            new_frame_info.sf = sf;
            new_frame_info.samp_rate = m_samp_rate;
            new_frame_info.bandwidth = m_bw;
            new_frame_info.time_full = time_full;
            new_frame_info.time_frac = time_frac;


            m_latest_frame_full_time = time_full;

            m_current_time = ::uhd::time_spec_t(time_full,time_frac);
            debug_print("Set time from tag: "<< m_current_time.get_full_secs()<<" "<<m_current_time.get_frac_secs(),m_identity,"rrh_zmq_sink",YELLOW);

            double offset = (double(nitems_read(0))-tags[0].offset )/(float)m_samp_rate;
            debug_print("Offset = "<<offset<<", read: "<<nitems_read(0)<<", offse: "<<tags[0].offset,m_identity,"rrh_zmq_sink",YELLOW);
            m_current_time+= offset;

            debug_print("Set time from tag: "<< m_current_time.get_full_secs()<<" "<<m_current_time.get_frac_secs(),m_identity,"rrh_zmq_sink",YELLOW);


            m_frame_list[ch].push_back(new_frame_info); //
            // m_streaming_channels[ch] = NEW_FRAME;
            // debug_print_rrh("New frame on sf" << sf )

            // debug_print_rrh("Frame header: " << std::hex << "0x" << m_new_frame_list.back() << std::dec )
          }
        }
      }
      std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      for (int stream_ch = 0; stream_ch < m_frame_list.size(); stream_ch++)
      {
        if (m_frame_list[stream_ch].size() > 0) // we should push the samples
        {
            // debug_print("samples header:",m_identity,"rrh_zmq_sink",YELLOW);
          
          zmq::send_result_t bytes_sent;
          for (int i = 0; i < m_frame_list[stream_ch].size(); i++) // send new frame list
          {
            // if (m_frame_list[stream_ch][i].frame_state == NEW_FRAME)
            // {
            // send channel identifier
            zmq::message_t message(sizeof(m_frame_list[stream_ch][0]));
            memcpy(message.data(), &m_frame_list[stream_ch][i], sizeof(m_frame_list[stream_ch][i]));
            bytes_sent = m_push_socket.send(message, zmq::send_flags::sndmore);
            assert(*bytes_sent != -1);
            // debug_print("header: frame_state " << m_frame_list[stream_ch][i].frame_state
            //                                        << ", channel_id " << (int)m_frame_list[stream_ch][i].channel_id
            //                                        << ", sf " << (int)m_frame_list[stream_ch][i].sf
            //                                        << ", time: " << std::dec << m_frame_list[stream_ch][i].time_full << " " << m_frame_list[stream_ch][i].time_frac << std::dec,m_identity,"rrh_zmq_sink",YELLOW);
            
            
            // change frame state to continue if it was new
            if (m_frame_list[stream_ch][i].frame_state == NEW_FRAME)
            {
              m_frame_list[stream_ch][i].frame_state = CONTINUE_FRAME;
            }
            // }
            // else if (m_streaming_channels[stream_ch][i].frame_state == CONTINUE_WORK) // send streaming info
            // {
            //   uint32_t frame_pre = (get_channel_id(center_freq_MHz[m_frequency_indices[stream_ch]]) << 1) + STREAMING;
            //   zmq::message_t message(sizeof(frame_pre));

            //   memcpy(message.data(), &frame_pre, sizeof(frame_pre));
            //   bytes_sent = m_push_socket.send(message, zmq::send_flags::sndmore);
            //   assert(*bytes_sent != -1);
            //   debug_print_rrh("continue: 0x" << std::hex << frame_pre << std::dec);
            // }

            //check if there is a timeout for this frame
            if(m_latest_frame_full_time-m_frame_list[stream_ch][i].time_full > FRAME_DETECT_TIME_OUT && m_frame_list[stream_ch][i].time_full < m_latest_frame_full_time)
            {
              m_frame_list[stream_ch].erase(m_frame_list[stream_ch].begin()+i);
              i--;
            }
          }
          // send empty delimiter
          zmq::message_t empty_msg(0);
          // debug_print("empty delimiter",m_identity,"rrh_zmq_sink",YELLOW);
          bytes_sent = m_push_socket.send(empty_msg, zmq::send_flags::sndmore);
          assert(*bytes_sent != -1);

          // debug_print("First_sample time: "<< m_current_time.get_full_secs()<<" "<<m_current_time.get_frac_secs(),m_identity,"rrh_zmq_sink",YELLOW);

          //send time of first sample
          zmq::message_t time_msg(&m_current_time,sizeof(m_current_time));

          bytes_sent = m_push_socket.send(time_msg, zmq::send_flags::sndmore);
          assert(*bytes_sent != -1);

          // send samples
          size_t payload_len = noutput_items * sizeof(gr_complex);
          m_current_time += double(noutput_items)/m_samp_rate; //TODO fix for multi channel inputs
          zmq::message_t msg(payload_len);
          // TODO only for debugging, prefer memcpy
          for (int i = 0; i < noutput_items; i++)
          {
            // debug_print_rrh(i<<"/"<<noutput_items<<" "<<msg.size());
            // static_cast<gr_complex*>(msg.data())[i] =  m_debug_vect[m_offset[stream_ch]%m_debug_vect.size()];
            static_cast<gr_complex *>(msg.data())[i] = static_cast<const gr_complex *>(input_items[stream_ch])[i];
            m_offset[stream_ch]++;
          }

          //--------
          // memcpy(msg.data(), input_items[stream_ch], payload_len);
          bytes_sent = m_push_socket.send(msg, zmq::send_flags::none);
          assert(*bytes_sent != -1);
          // debug_print("sent "<<*bytes_sent << "B of payloads",m_identity,"rrh_zmq_sink",YELLOW);

          last_heartbeat = std::chrono::steady_clock::now();
        }
      }
      // keep buffer alive if needed
      // std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat).count() > BUFFER_HEARTBEAT)
      {
        s_send(m_push_socket, ""); // send message to keep buffer alive
        last_heartbeat = std::chrono::steady_clock::now();
      }

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace cran */
} /* namespace gr */
