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

#ifndef INCLUDED_CRAN_WORKER_ZMQ_SOURCE_IMPL_H
#define INCLUDED_CRAN_WORKER_ZMQ_SOURCE_IMPL_H

#include <gnuradio/cran/worker_zmq_source.h>
#include <zmq.hpp>
#include "gnuradio/cran/zhelper.h"
#include "gnuradio/cran/utilities.h"
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <boost/algorithm/string.hpp>
#include <uhd/types/time_spec.hpp>
#include <fstream>


#define SUBSCRIPTION_TIMEOUT 4000
#define IDLE_TIMEOUT 20000 //timeout without receiving a heartbeat

#define SYMB_PRETRIGGER 10 //Number of symbols  to consider before the preamble detection of the RRH triggered

#define PREAMBLE_DETECTION_TIMEOUT 4096*200 //Number of output samples to find preamble before giving up


namespace gr {
  namespace cran {

    class worker_zmq_source_impl : public worker_zmq_source
    {
     private:
       enum State
      {
        IDLE,
        WORKING,
      };
      char m_identity[10] = {}; //unique identifier
      bool m_can_die;
      std::string m_broker_addr;
      std::string m_buffer_addr;
      State m_state;
      // uint8_t m_sf;
      // float m_fc;
      // uint16_t m_channel;
      zmq::message_t m_msg_buff;
      int m_consumed_bytes;
      float m_symb_cnt;
      int m_nitems_per_symb;
      bool m_preamble_found;
      uint32_t m_samp_rate;

      bool m_is_new_work;

      int m_delay_cnt;

      pmt::pmt_t new_frame_tag; ///< tag added to the first sample of a new work


      // initialize the zmq context with a single IO thread
      zmq::context_t context{1};

      zmq::socket_t m_broker_socket{context, zmq::socket_type::dealer};
      zmq::socket_t m_buffer_samples_socket{context, zmq::socket_type::sub};

      std::string m_buffer_req_addr{};
      std::string m_empty{};
      std::string m_rrh_req_info{};
      std::string m_samples{};
      std::string m_rrh_addr{};
      gr::cran::Work_info m_work_info; 
      std::vector<std::string> m_infos;

      void frame_end_handler(pmt::pmt_t frame_end);
      void preamb_info_handler(pmt::pmt_t preamb_info);
      void free_worker_for_new_work(std::string status_msg, bool notify_borker=true);
      void kill_worker(std::string status_msg, bool notify_borker=true);
     public:
      worker_zmq_source_impl(std::string broker_addr);
      ~worker_zmq_source_impl();

      // Where all the action really happens
      int work(
              int noutput_items,
              gr_vector_const_void_star &input_items,
              gr_vector_void_star &output_items
      );
    };

  } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_WORKER_ZMQ_SOURCE_IMPL_H */

