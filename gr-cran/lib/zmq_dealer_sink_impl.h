/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_ZMQ_DEALER_SINK_IMPL_H
#define INCLUDED_CRAN_ZMQ_DEALER_SINK_IMPL_H


#define GR_HEADER_MAGIC 0x5FF0
#define GR_HEADER_VERSION 0x01

#include <gnuradio/cran/zmq_dealer_sink.h>
#include <zmq.hpp>
#include "gnuradio/cran/utilities.h"
#include "gnuradio/cran/zhelper.h"

namespace gr {
namespace cran {

class zmq_dealer_sink_impl : public zmq_dealer_sink {
private:
  char m_identity[10] = {}; //unique identifier

    //initialize zmq
  zmq::context_t m_context{1};
  zmq::socket_t m_out_socket{m_context, zmq::socket_type::dealer};
  int send_message(const void* in_buf, const int in_nitems, const uint64_t in_offset);
  bool m_work_done=false;
  std::string m_dest_addr;
  void  work_done_handler(pmt::pmt_t msg);
  
public:
  zmq_dealer_sink_impl();
  ~zmq_dealer_sink_impl();

  // Where all the action really happens
  int work(int noutput_items, gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_ZMQ_DEALER_SINK_IMPL_H */
