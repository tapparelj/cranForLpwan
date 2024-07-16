/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_WORKER_ZMQ_SINK_IMPL_H
#define INCLUDED_CRAN_WORKER_ZMQ_SINK_IMPL_H


#include <gnuradio/cran/worker_zmq_sink.h>
#include <zmq.hpp>
#include "gnuradio/cran/zhelper.h"
#include "gnuradio/cran/utilities.h"

namespace gr
{
  namespace cran
  {

    class worker_zmq_sink_impl : public worker_zmq_sink
    {
    private:
      char m_identity[10] = {}; //unique identifier
  
      uint16_t m_work_id;
      uint32_t m_pay_len;
      int m_has_header_error;
      bool m_crc_valid;
      float m_snr;
      uint8_t m_sf;
      uint8_t m_cr;
      double m_time_frac;
      uint64_t m_time_full; 


      uint8_t m_n_rrh_involved;
      pmt::pmt_t m_rrh_info_vect;
      zmq::context_t context{1};
      zmq::socket_t m_broker_socket{context, zmq::socket_type::dealer};

      zmq::socket_t m_application_socket{context, zmq::socket_type::push};

      // void payload_handler(pmt::pmt_t msg);

    public:
      worker_zmq_sink_impl(std::string broker_addr, std::string application_addr);
      ~worker_zmq_sink_impl();

      // Where all the action really happens
      int work(int noutput_items, gr_vector_const_void_star &input_items,
               gr_vector_void_star &output_items);
    };

  } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_WORKER_ZMQ_SINK_IMPL_H */
