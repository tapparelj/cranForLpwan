/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zmq_dealer_sink_impl.h"
#include <gnuradio/io_signature.h>

namespace gr
{
  namespace cran
  {

    using input_type = gr_complex;
    zmq_dealer_sink::sptr zmq_dealer_sink::make()
    {
      return gnuradio::make_block_sptr<zmq_dealer_sink_impl>();
    }

    /*
     * The private constructor
     */
    zmq_dealer_sink_impl::zmq_dealer_sink_impl()
        : gr::sync_block("zmq_dealer_sink",
                         gr::io_signature::make(1, 1, sizeof(input_type)),
                         gr::io_signature::make(0, 0, 0))
    {
      // generate random identity
      
      sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));
      m_out_socket.setsockopt(ZMQ_IDENTITY, m_identity, strlen(m_identity));
      m_out_socket.setsockopt(ZMQ_SNDHWM, 2);
      m_out_socket.set(zmq::sockopt::linger, 0);


      //m_out_socket.connect(dest_addr);
      message_port_register_in(pmt::mp("work_done"));
      set_msg_handler(pmt::mp("work_done"), [this](pmt::pmt_t msg)
                      { this->work_done_handler(msg); });

      // m_dest_addr = dest_addr;
      debug_print("new zmqdealer ",m_identity,"zmq_dealer_sink",BLUE);
      m_dest_addr = "";
    }

    /*
     * Our virtual destructor.
     */
    zmq_dealer_sink_impl::~zmq_dealer_sink_impl()
    {
    }

    std::string gen_tag_header(uint64_t offset, std::vector<gr::tag_t> &tags)
    {
      std::stringbuf sb("");
      std::ostream ss(&sb);

      uint16_t header_magic = GR_HEADER_MAGIC;
      uint8_t header_version = GR_HEADER_VERSION;
      uint64_t ntags = (uint64_t)tags.size();

      ss.write((const char *)&header_magic, sizeof(uint16_t));
      ss.write((const char *)&header_version, sizeof(uint8_t));
      ss.write((const char *)&offset, sizeof(uint64_t));
      ss.write((const char *)&ntags, sizeof(uint64_t));

      for (size_t i = 0; i < tags.size(); i++)
      {
        ss.write((const char *)&tags[i].offset, sizeof(uint64_t));
        pmt::serialize(tags[i].key, sb);
        pmt::serialize(tags[i].value, sb);
        pmt::serialize(tags[i].srcid, sb);
      }
      return sb.str();
    }

    int zmq_dealer_sink_impl::send_message(const void *in_buf,
                                           const int in_nitems,
                                           const uint64_t in_offset)
    {
      // Meta-data header 
      std::string header("");

      std::vector<gr::tag_t> tags;
      get_tags_in_range(tags, 0, in_offset, in_offset + in_nitems);
      header = gen_tag_header(in_offset, tags);

      // Create message 
      size_t payload_len = in_nitems * sizeof(gr_complex);
      size_t msg_len = payload_len + header.length();
      zmq::message_t msg(msg_len);

      memcpy(msg.data(), header.c_str(), header.length());
      memcpy((uint8_t *)msg.data() + header.length(), in_buf, payload_len);

      // Send 
      m_out_socket.send(msg, zmq::send_flags::none);
      // debug_print("ID "<<m_identity<<" zmqdealer sent " << payload_len << " B to "<<m_dest_addr);
      // Report back 
      return in_nitems;
    }
    void zmq_dealer_sink_impl::work_done_handler(pmt::pmt_t msg)
    {
      
      if (nitems_read(0)==0){
        debug_print("work done from unfound preamble"<<nitems_read(0),m_identity,"zmq_dealer_sink",BLUE);
        s_send(m_out_socket,"",zmq::send_flags::dontwait);
      }
      m_work_done = true;
    }

    int zmq_dealer_sink_impl::work(int noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star &output_items)
    {
      auto in = static_cast<const input_type *>(input_items[0]);      

      if (m_work_done )
      {
        return WORK_DONE;
      }
      // read tags
      std::vector<tag_t> tags;
              
      get_tags_in_window(tags, 0, 0, noutput_items, pmt::string_to_symbol("frame_info"));
      if (tags.size())
      {
        pmt::pmt_t err = pmt::string_to_symbol("error");
        std::string demod_worker_addr = pmt::symbol_to_string(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("demod_worker_addr"), err));
        uint16_t new_work_id = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("work_id"), err));
        debug_print("[Time] got frame info at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","zmq_Dealer sink",GREEN);

        
        // check if we need to connect to a new demod worker
        if(demod_worker_addr.compare(m_dest_addr) != 0)
        {
          debug_print("Work ID "<< new_work_id <<", we need to disconnect from "<< m_dest_addr<<" and connect to new demod worker address "<<demod_worker_addr,m_identity,"zmq_dealer_sink",CYAN); 
          if(m_dest_addr.size() > 0){
            m_out_socket.disconnect(m_dest_addr);
          }
          m_dest_addr = demod_worker_addr;
          m_out_socket.connect(m_dest_addr);
        }
        else{
          debug_print("Work ID "<< new_work_id <<", already connected to demod worker address "<<demod_worker_addr,m_identity,"zmq_dealer_sink",CYAN); 
        }
      }

      // Poll with a timeout (FIXME: scheduler can't wait for us)
      zmq::pollitem_t itemsout[] = {{static_cast<void *>(m_out_socket), 0, ZMQ_POLLOUT, 0}};
      zmq::poll(&itemsout[0], 1, -1);

      //If we can send something, do it
      if (itemsout[0].revents & ZMQ_POLLOUT)
      {
        int sent_items = send_message(input_items[0], noutput_items, nitems_read(0)); 
        debug_print("[Time] sends " << sent_items <<" items at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","zmq_Dealer sink",GREEN);

        return sent_items;
      }
      // If not, do nothing
      return 0;
    }

  } /* namespace cran */
} /* namespace gr */
