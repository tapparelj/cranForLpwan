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
#include "worker_zmq_source_impl.h"

namespace gr
{
  namespace cran
  {

    worker_zmq_source::sptr
    worker_zmq_source::make(std::string broker_addr)
    {
      return gnuradio::get_initial_sptr(new worker_zmq_source_impl(broker_addr));
    }

    /*
     * The private constructor
     */
    worker_zmq_source_impl::worker_zmq_source_impl(std::string broker_addr)
        : gr::sync_block("worker_zmq_source",
                         gr::io_signature::make(0, 0, 0),
                         gr::io_signature::make(1, 1, sizeof(gr_complex)))
    {
      sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));

      m_broker_addr = broker_addr;
      m_state = IDLE;
      // zmq bindings
      m_broker_socket.connect(m_broker_addr);
      m_delay_cnt = 0;

      debug_print("Worker sending Ready ...", m_identity, "worker_zmq_source", YELLOW);
      s_sendmore(m_broker_socket, "");
      s_send(m_broker_socket, "READY");
      m_consumed_bytes = 0;
      m_can_die = false;

      // register message port
      // message_port_register_in(pmt::mp("frame_end"));
      // set_msg_handler(pmt::mp("frame_end"), [this](pmt::pmt_t msg)
      //                 { this->frame_end_handler(msg); });
      message_port_register_in(pmt::mp("preamb_info"));
      set_msg_handler(pmt::mp("preamb_info"), [this](pmt::pmt_t msg)
                      { this->preamb_info_handler(msg); });

      message_port_register_out(pmt::mp("system_out"));

    }

    void worker_zmq_source_impl::kill_worker(std::string status_msg, bool notify_broker)
    {
      debug_print(status_msg << ", Worker can die for work " << m_work_info.work_id, m_identity, "worker_zmq_source", YELLOW);
      message_port_pub(pmt::mp("system_out"), pmt::cons(pmt::intern("done"), pmt::from_long(1)));
      if (notify_broker)
      {
        s_sendmore(m_broker_socket, "");
        s_sendmore(m_broker_socket, "SYNC_DONE");

        zmq::message_t message(sizeof(m_work_info.work_id));
        memcpy(message.data(), &m_work_info, sizeof(m_work_info.work_id));
        m_broker_socket.send(message, zmq::send_flags::none);

        m_broker_socket.close();
      }
      m_buffer_samples_socket.disconnect(m_buffer_addr);
      m_can_die = true;
    }

    void worker_zmq_source_impl::free_worker_for_new_work(std::string status_msg, bool notify_broker)
    {
      debug_print(status_msg << ", work "<< m_work_info.work_id<<" done, Worker free for new work " , m_identity, "worker_zmq_source", CYAN);
      if (notify_broker)
      {
        s_sendmore(m_broker_socket, "");
        s_sendmore(m_broker_socket, "SYNC_DONE");

        zmq::message_t message(sizeof(m_work_info.work_id));
        memcpy(message.data(), &m_work_info, sizeof(m_work_info.work_id));
        m_broker_socket.send(message, zmq::send_flags::none);
      }
      m_buffer_samples_socket.disconnect(m_buffer_addr);
      m_state = IDLE;
      s_sendmore(m_broker_socket, "");
      s_send(m_broker_socket, "READY");
      m_msg_buff.rebuild(0);
      m_consumed_bytes = 0;

      //  char channel[sizeof(m_channel)];
      //  memcpy(channel, &m_channel, sizeof(m_channel));
      //  s_sendmore(m_broker_socket, channel);
      //  s_sendmore(m_broker_socket, m_buffer_req_addr);
      //  s_sendmore(m_broker_socket, "");
      //  s_send(m_broker_socket, "SUCCESS");
      //  debug_print(status_msg << ", unsubscribing from " << string_to_hex(m_buffer_req_addr) << ", channel " << m_channel);
    }
    /*
     * Our virtual destructor.
     */
    worker_zmq_source_impl::~worker_zmq_source_impl()
    {
    }

    void worker_zmq_source_impl::preamb_info_handler(pmt::pmt_t preamb_info)
    {
      bool new_preamb = pmt::to_bool(pmt::dict_ref(preamb_info, pmt::string_to_symbol("new_preamb"), pmt::string_to_symbol("err")));
      debug_print("Channel " << (int)m_work_info.channel_id << ": preamble found after " << m_symb_cnt << " processed symbols", m_identity, "worker_zmq_source", YELLOW);
      if (new_preamb)
      {
        m_preamble_found = true;
      }
    }

    int worker_zmq_source_impl::work(int noutput_items,
                                     gr_vector_const_void_star &input_items,
                                     gr_vector_void_star &output_items)
    {
      gr_complex *out = (gr_complex *)output_items[0];


      std::string ack;
      zmq::pollitem_t items[2] = {{m_broker_socket, 0, ZMQ_POLLIN, 0},
                                  {m_buffer_samples_socket, 0, ZMQ_POLLIN, 0}};
      int nitems_to_output = 0;
      if (m_can_die)
      {
        return WORK_DONE;
      }
      while (1)
      {
        if (m_state == IDLE)
        {
          // wait for reply from server
          debug_print("Worker waiting for work ...", m_identity, "worker_zmq_source", YELLOW);

          // poll for work or keep-alive
          // items[3] =
          zmq::poll(&items[0], 1, IDLE_TIMEOUT);
          if (items[0].revents & ZMQ_POLLIN)
          {
            std::string empty = s_recv(m_broker_socket);
            if (empty.size() != 0)
            {
              error_print("It's supposed to be empty!! instead of : " << empty, m_identity);
            }; // assert
            m_buffer_req_addr = s_recv(m_broker_socket);
            // check if it is a keep-alive ack or a RRH request for work

            if (m_buffer_req_addr.compare("ACK") == 0)
            {// we receive an ack from broker to stay alive (not used currently)
              debug_print("Staying alive..", m_identity, "worker_zmq_source", YELLOW);
              s_sendmore(m_broker_socket, "");
              s_send(m_broker_socket, "READY");
            }
            else if (m_buffer_req_addr.compare("DONE") == 0)
            {// we shouldn't receive DONE before receiving a new work
              zmq::message_t work_id_done_msg(sizeof(uint16_t));
              uint16_t work_id_done;
              m_broker_socket.recv(work_id_done_msg);
              memcpy( &work_id_done,work_id_done_msg.data(), sizeof(uint16_t));
              debug_print("We received DONE for work "<<work_id_done<<"which should have ended due to no preamble found", m_identity, "worker_zmq_source", YELLOW);
            }
            else 
            { // we receive some work from rrh [empty][work info][buffer addr]
              m_empty = s_recv(m_broker_socket);
              assert(m_empty.size() == 0);
              // m_empty = s_recv(m_broker_socket);
              // assert(m_empty.size() == 0);

              zmq::message_t msg_buff;
              m_broker_socket.recv(msg_buff);
              if(msg_buff.size() != sizeof(gr::cran::Work_info)){
                error_print("Received work info of size: "<<msg_buff.size() <<" instead of "<<sizeof(gr::cran::Work_info), m_identity);
                //print content as a string
                size_t size = msg_buff.size();
                char *string = (char*)malloc(size + 1);
                memcpy(string, msg_buff.data(), size);
                string[size] = 0;
                debug_print("received hex values: "<<string_to_hex(string), m_identity, "worker_zmq_source", YELLOW);
              }
              memcpy(&m_work_info, (gr::cran::Work_info *)msg_buff.data(), sizeof(gr::cran::Work_info));
              // m_frame_info = (gr::cran::Frame_info *)msg_buff.data();

              // std::string header = s_recv(m_broker_socket);
              // uint16_t *frame_info = (uint16_t *)header.data();
              //*frame_info >> 1;
              // parse info from channel
              // m_sf = m_frame_info.sf;
              // m_channel = header->channel_id;
              m_nitems_per_symb = (1u << m_work_info.sf);
              set_max_output_buffer((1u << m_work_info.sf) * 10);
              m_samp_rate = m_work_info.samp_rate;

              m_symb_cnt = 0;
              m_preamble_found = false;

              std::stringstream ss;
              ss << m_work_info.channel_id;
              std::string topic = ss.str();

              m_buffer_addr = s_recv(m_broker_socket);
              debug_print("New work " << m_work_info.work_id << ", sf= " << (int)m_work_info.sf << ", channel_id= " << (int)m_work_info.channel_id
                                      << ", time: " << m_work_info.time_full << " " << m_work_info.time_frac << ", demod worker address: " << m_work_info.demod_worker_addr
                                      << " from " << string_to_hex(m_buffer_req_addr),
                          m_identity, "worker_zmq_source", YELLOW);
              debug_print("Subscribing to " << m_buffer_addr << " on topic " << (int)m_work_info.channel_id << std::dec, m_identity, "worker_zmq_source", YELLOW);
              m_buffer_samples_socket.connect(m_buffer_addr);

              m_buffer_samples_socket.set(zmq::sockopt::subscribe, topic.c_str());

              usleep(10000); // leave some time to finalise the sub sockets connection //TODO!! reduce delay after debug
              s_sendmore(m_broker_socket, "");
              s_sendmore(m_broker_socket, m_buffer_req_addr);
              s_sendmore(m_broker_socket, "");
              s_send(m_broker_socket, "SUBSCRIBED");

              // ack = s_recv(m_broker_socket);
              // assert(ack.compare("ACK") == 0);

              new_frame_tag = pmt::make_dict();
              float center_freq = center_freq_MHz[m_work_info.channel_id];
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("work_id"), pmt::mp((long)m_work_info.work_id));
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("buffer_addr"), pmt::mp(m_buffer_addr));
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("fc"), pmt::mp((float)center_freq));
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("sf"), pmt::mp((long)m_work_info.sf));
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("demod_worker_addr"), pmt::mp(m_work_info.demod_worker_addr));
              new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("is_end"), pmt::from_bool(false));

              // add_item_tag(0, nitems_written(0), pmt::string_to_symbol("new_frame"), new_frame_tag);

              m_state = WORKING;
              m_is_new_work = true;
            }
          
          }
          else // time out
          {
            //debug_print("Worker can die", m_identity, "worker_zmq_source", YELLOW);
            //m_broker_socket.close();
            return 0;
          }
        }
        else if (m_state == WORKING)
        {
          zmq::poll(items, 2, std::chrono::milliseconds(SUBSCRIPTION_TIMEOUT));
          if (items[0].revents & ZMQ_POLLIN) // broker indicates end on work
          {
            std::string client_addr = s_recv(m_broker_socket);
            if (client_addr.compare("DONE") == 0)
            {
              zmq::message_t work_id_done_msg(sizeof(uint16_t));
              uint16_t work_id_done;
              m_broker_socket.recv(work_id_done_msg);
              memcpy(&work_id_done, work_id_done_msg.data(),sizeof(uint16_t));

              debug_print("Received DONE from broker for work "<<work_id_done, m_identity, "worker_zmq_source", YELLOW);
              free_worker_for_new_work("Success", false);
              break;
            }
          }
          // if we have consumed all previous samples
          if (m_msg_buff.size() <= m_consumed_bytes)
          {
            if (items[1].revents & ZMQ_POLLIN)
            { // we received data
              zmq::recv_result_t msg_size = m_buffer_samples_socket.recv(m_msg_buff);
              assert(msg_size != -1);
              
              m_consumed_bytes = 1; // topic already consumed

              // read time
              ::uhd::time_spec_t first_sample_time;

              memcpy(&first_sample_time, m_msg_buff.data() + m_consumed_bytes, sizeof(::uhd::time_spec_t));
              m_consumed_bytes += sizeof(::uhd::time_spec_t); // consumes the time

              // don't output samples that are too early compared to the work start time
              ::uhd::time_spec_t work_time(m_work_info.time_full, m_work_info.time_frac);

              ::uhd::time_spec_t time_diff(first_sample_time - work_time);

              double samp_diff = int(double(time_diff.get_full_secs() + time_diff.get_frac_secs()) * m_samp_rate);

              int samp_to_skip = std::max(0.0, -(samp_diff + SYMB_PRETRIGGER * (1u << m_work_info.sf)));
              debug_print("[Debug] work id "<<m_work_info.work_id <<", First sample time: " << first_sample_time.get_full_secs() << " " << first_sample_time.get_frac_secs() << ", time diff: " << time_diff.get_full_secs() + time_diff.get_frac_secs()<<" skip "<<samp_to_skip<<" samples", m_identity, "worker_zmq_source", YELLOW);

              if (samp_to_skip > 0)
              {
                debug_print("work id "<<m_work_info.work_id <<", Samp_diff: " << samp_diff << ", we should skip: " << samp_to_skip << " samples", m_identity, "worker_zmq_source", YELLOW);
                first_sample_time = first_sample_time + ::uhd::time_spec_t(0, samp_to_skip * (double)1.0/m_samp_rate);
              }
              m_consumed_bytes += samp_to_skip * sizeof(gr_complex);

              //Add tag with time at the start of the work
              if(m_is_new_work){
                new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("time_full"), pmt::mp(first_sample_time.get_full_secs()));
                new_frame_tag = pmt::dict_add(new_frame_tag, pmt::intern("time_frac"), pmt::mp(first_sample_time.get_frac_secs()));

                debug_print("work id "<<m_work_info.work_id <<", First sample time: " << first_sample_time.get_full_secs() << " " << first_sample_time.get_frac_secs()<<" nitems_written(0) "<<nitems_written(0), m_identity, "worker_zmq_source", YELLOW);

                add_item_tag(0, nitems_written(0), pmt::string_to_symbol("work_info"), new_frame_tag);
                m_is_new_work = false;
              }
              // debug_print(" received " << m_msg_buff.size() << "B",m_identity,"worker_zmq_source",YELLOW); // << std::string(static_cast<char*>(m_msg_buff.data()), m_msg_buff.size()));
            }
            else // we have a timeout from buffer
            {
              free_worker_for_new_work("Timeout, no samples received from buffer");
              // m_broker_socket.close();
              // m_buffer_samples_socket.disconnect(m_buffer_addr);
              // return WORK_DONE;
              break;
            }
          }
          else // empty buffer as much as possible
          {
            // check for preamble detection timeout
            // debug_print( "ID"<< " m_symb_cnt "<<m_symb_cnt<<" / "<<(PREAMBLE_DETECTION_TIMEOUT / m_nitems_per_symb)<< " m_preamble_found " <<m_preamble_found<< RESET);

            if ((m_symb_cnt > (PREAMBLE_DETECTION_TIMEOUT / m_nitems_per_symb)) && !m_preamble_found)
            {
              warning_print("work id "<<m_work_info.work_id <<", Timeout without finding preamble in " << PREAMBLE_DETECTION_TIMEOUT << " samples ", m_identity);
              free_worker_for_new_work("Timeout without finding preamble");
            }
            int to_copy_bytes = std::min((int)sizeof(gr_complex) * (noutput_items - 1), (int)(m_msg_buff.size() - m_consumed_bytes)); // set noutput_items-1 such that this work function will always be called after return
            memcpy(out, (uint8_t *)m_msg_buff.data() + m_consumed_bytes, to_copy_bytes * sizeof(uint8_t));
            m_consumed_bytes += to_copy_bytes;

            nitems_to_output += (int)(to_copy_bytes / sizeof(gr_complex));
            m_symb_cnt += nitems_to_output / m_nitems_per_symb;
            // debug_print("ID" <<m_id <<" Copy to ouput: " << (int)to_copy_bytes << " bytes, nitems: " << nitems_to_output);
            break;
          }
        }
      }
      // Tell runtime system how many output items we produced.
      debug_print("zmq source output " << nitems_to_output << "items",m_identity, "worker_zmq_source", BLUE);
      return nitems_to_output;
    }

  } /* namespace cran */
} /* namespace gr */
