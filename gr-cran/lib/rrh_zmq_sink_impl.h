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

#ifndef INCLUDED_CRAN_RRH_ZMQ_SINK_IMPL_H
#define INCLUDED_CRAN_RRH_ZMQ_SINK_IMPL_H

#include <gnuradio/cran/rrh_zmq_sink.h>
#include <zmq.hpp>
#include "gnuradio/cran/zhelper.h"
#include <chrono>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include "gnuradio/cran/utilities.h"
#include <uhd/types/time_spec.hpp>



#define BUFFER_HEARTBEAT 5000
#define NEW_FRAME_MIN_DELAY 80 // in Symbols
#define FRAME_DETECT_TIME_OUT 10 //delay after starting streaming a frame before removing it from the list of active frame (in seconds)

namespace gr
{
  namespace cran
  {
    class rrh_zmq_sink_impl : public rrh_zmq_sink
    {
    private:
      char m_identity[10] = {}; //unique identifier

      float m_fc;
      int m_samp_rate;//in Hz
      std::string m_pub_addr;
      std::string m_broker_addr;
      uint m_bw = 125000; ///< channel bandwidth //TODO set as parameter
      // initialize zmq
      zmq::context_t m_context{1};
      // construct a REQ (request) socket and connect to interface
      zmq::socket_t *m_broker_socket; //{m_context, zmq::socket_type::req};
      zmq::socket_t m_push_socket{m_context, zmq::socket_type::push};
      bool m_need_new_socket;
      std::string m_buffer_addr;
      std::vector<std::vector<Frame_info>> m_frame_list; ///< list of frames for each carrier center frequency 
      std::chrono::steady_clock::time_point last_heartbeat;
      std::chrono::steady_clock::time_point start_time;
      std::vector<int> m_streaming_channels; ///< list of currently streaming channels
      std::vector<int> m_frequency_indices;  ///< list of center frequencies of this rrh

      uint64_t m_latest_frame_full_time; /// full time of the latest frame (used to identify timeout)

      ::uhd::time_spec_t m_current_time;//time of the first sample ready to send

      std::vector<std::pair<uint16_t, std::chrono::steady_clock::time_point>> work_list; ///< list of current work as <channel, minimum next starting time for work on the same channel>
      //TODO for debugging 
      std::vector<gr_complex> m_debug_vect{};
      std::vector<int> m_offset{0,0,0,0,0,0,0,0};


    public:
      rrh_zmq_sink_impl(std::string broker_addr, std::vector<int> frequency_indices, int samp_rate);
      ~rrh_zmq_sink_impl();
      void forecast(int noutput_items, gr_vector_int &ninput_items_required);


      // Where all the action really happens
      int work(
          int noutput_items,
          gr_vector_const_void_star &input_items,
          gr_vector_void_star &output_items);
    };

  } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_RRH_ZMQ_SINK_IMPL_H */
