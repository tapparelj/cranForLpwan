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

#ifndef INCLUDED_CRAN_END_NODE_ZMQ_SINK_IMPL_H
#define INCLUDED_CRAN_END_NODE_ZMQ_SINK_IMPL_H

#include <gnuradio/cran/end_node_zmq_sink.h>
#include <chrono>
#include "gnuradio/cran/utilities.h"
#include <zmq.hpp>
#include "gnuradio/cran/zhelper.h"

#define COLOR BLUE
#define FILENAME "end_node_zmq_sink"

namespace gr {
  namespace cran {

    class end_node_zmq_sink_impl : public end_node_zmq_sink
    {
     private:
      bool m_first_tag = true;
      uint64_t m_offset ;
      std::chrono::time_point<std::chrono::steady_clock> m_start;
      std::chrono::time_point<std::chrono::steady_clock> m_now;

      int m_samp_rate;

      zmq::context_t context{1};
      // construct a REQ (request) socket and connect to interface
      zmq::socket_t m_rrh_socket{context, zmq::socket_type::push};
      long m_frame_length;

     public:
      end_node_zmq_sink_impl(std::string rrh_addr, int samp_rate);
      ~end_node_zmq_sink_impl();

      // Where all the action really happens
      int work(
              int noutput_items,
              gr_vector_const_void_star &input_items,
              gr_vector_void_star &output_items
      );
    };

  } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_END_NODE_ZMQ_SINK_IMPL_H */

