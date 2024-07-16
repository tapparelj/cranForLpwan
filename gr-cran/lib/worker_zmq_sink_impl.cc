/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "worker_zmq_sink_impl.h"
#include <gnuradio/io_signature.h>

namespace gr
{
  namespace cran
  {

    using input_type = uint8_t;
    worker_zmq_sink::sptr worker_zmq_sink::make(std::string broker_addr,
                                                std::string application_addr)
    {
      return gnuradio::make_block_sptr<worker_zmq_sink_impl>(broker_addr,
                                                             application_addr);
    }

    /*
     * The private constructor
     */
    worker_zmq_sink_impl::worker_zmq_sink_impl(std::string broker_addr,
                                               std::string application_addr)
        : gr::sync_block(
              "worker_zmq_sink",
              gr::io_signature::make(1, 1, sizeof(input_type)),
              gr::io_signature::make(0, 0, 0))
    {
      m_application_socket.connect(application_addr);
      m_broker_socket.connect(broker_addr);
      sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));

      // register message port
      // message_port_register_in(pmt::mp("msg"));
      // set_msg_handler(pmt::mp("msg"), [this](pmt::pmt_t msg)
      //                 { this->payload_handler(msg); });
    }

    /*
     * Our virtual destructor.
     */
    worker_zmq_sink_impl::~worker_zmq_sink_impl() {}

    // void worker_zmq_sink_impl::payload_handler(pmt::pmt_t msg)
    // {
    //   debug_print("got one frame!");
    //   std::stringbuf sb("");
    //   pmt::serialize(msg, sb);
    //   std::string s = sb.str();
    //   zmq::message_t zmsg(s.size());
    //   memcpy(zmsg.data(), s.c_str(), s.size());
    //   m_application_socket.send(zmsg, zmq::send_flags::none);

    //   s_sendmore(m_broker_socket, "DONE");
    //   zmq::message_t message(sizeof(m_work_id));
    //   memcpy(message.data(), &m_work_id, sizeof(m_work_id));
    //   m_broker_socket.send(message, zmq::send_flags::none);
    // }

    int worker_zmq_sink_impl::work(int noutput_items,
                                   gr_vector_const_void_star &input_items,
                                   gr_vector_void_star &output_items)
    {
      auto in = static_cast<const input_type *>(input_items[0]);
      std::vector<tag_t> tags;
      get_tags_in_window(tags, 0, 0, noutput_items, pmt::string_to_symbol("frame_info"));
      if (tags.size())
      {
        pmt::pmt_t err = pmt::string_to_symbol("error");
        m_work_id = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("work_id"), err));
        m_crc_valid = pmt::to_bool(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("crc_valid"), pmt::from_bool(false)));
        m_pay_len = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("pay_len"), err));
        m_has_header_error = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("err"), err));
        m_rrh_info_vect = pmt::dict_ref(tags[0].value, pmt::string_to_symbol("rrh_info_vect"), err);
        m_cr = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("cr"), err));
        m_sf = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("sf"), err));
        m_n_rrh_involved = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("n_rrh_involved"), err));

        debug_print("got frame tag: work ID= " << m_work_id << ", pay_len= " << m_pay_len << ", crc valid? " << m_crc_valid << " rrh infos:  " << m_rrh_info_vect,m_identity,"worker_zmq_sink",MAGENTA);
      }
      if (m_has_header_error)
      {        
        s_sendmore(m_broker_socket, "");
        s_sendmore(m_broker_socket, "DEMOD_DONE");
        zmq::message_t message(sizeof(m_work_id));
        memcpy(message.data(), &m_work_id, sizeof(m_work_id));
        m_broker_socket.send(message, zmq::send_flags::none);
        debug_print(" invalid header, send work done to broker",m_identity,"worker_zmq_sink",MAGENTA);
        consume_each(noutput_items);

      }
      else if (noutput_items >= m_pay_len)
      {
        debug_print("sends " << m_pay_len << " bytes",m_identity,"worker_zmq_sink",MAGENTA);
        // ------------ send to application ---------------

        // CRC result
        zmq::message_t msg(sizeof(m_crc_valid));
        memcpy(msg.data(), &m_crc_valid, sizeof(m_crc_valid));
        m_application_socket.send(msg,zmq::send_flags::sndmore);

        // SF
        msg.rebuild(sizeof(m_sf));
        memcpy(msg.data(), &m_sf, sizeof(m_sf));
        m_application_socket.send(msg,zmq::send_flags::sndmore);

        // code rate
        msg.rebuild(sizeof(m_cr));
        memcpy(msg.data(), &m_cr, sizeof(m_cr));
        m_application_socket.send(msg,zmq::send_flags::sndmore);

        // number of rrh involved
        msg.rebuild(sizeof(m_n_rrh_involved));
        memcpy(msg.data(), &m_n_rrh_involved, sizeof(m_n_rrh_involved));
        m_application_socket.send(msg,zmq::send_flags::sndmore);

        // serialized rrh infos
        s_sendmore(m_application_socket,pmt::serialize_str(m_rrh_info_vect));

        // all buffer addresses involved
        // std::string buffer_addr;
        // for (int ii = 0; ii < m_n_rrh_involved; ii++) 
        // {
        //   // buffer_addr = pmt::symbol_to_string(pmt::vector_ref(m_buffer_addr,ii));
        //   // if(buffer_addr.compare("") == 0)
        //   // {
        //   //   break;
        //   // }
        //   // else
        //   // {
        //   //   s_sendmore(m_application_socket, buffer_addr);
        //   // }
        // }

        // empty delimiter
        // s_sendmore(m_application_socket, "");
        // payload
        zmq::message_t zmsg(m_pay_len);
        memcpy(zmsg.data(), &in[0], m_pay_len);
        m_application_socket.send(zmsg, zmq::send_flags::none);

        // ----------- notify broker that we are done ----------------
        s_sendmore(m_broker_socket, "");
        s_sendmore(m_broker_socket, "DEMOD_DONE");
        zmq::message_t message(sizeof(m_work_id));
        memcpy(message.data(), &m_work_id, sizeof(m_work_id));
        m_broker_socket.send(message, zmq::send_flags::none);
        debug_print(" send work done to broker",m_identity,"worker_zmq_sink",MAGENTA);

        consume_each(noutput_items);
        return 0;
      }
      return 0;

      // Tell runtime system how many output items we produced.
    }

  } /* namespace cran */
} /* namespace gr */
