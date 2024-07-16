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
#include "end_node_zmq_sink_impl.h"

namespace gr {
  namespace cran {

    end_node_zmq_sink::sptr
    end_node_zmq_sink::make(std::string rrh_addr, int samp_rate)
    {
      return gnuradio::get_initial_sptr
        (new end_node_zmq_sink_impl(rrh_addr, samp_rate));
    }


    /*
     * The private constructor
     */
    end_node_zmq_sink_impl::end_node_zmq_sink_impl(std::string rrh_addr, int samp_rate)
      : gr::sync_block("end_node_zmq_sink",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(0, 0, 0))
    {
      m_rrh_socket.connect(rrh_addr);
      m_samp_rate = samp_rate;
      // debug_print("connect to " << rrh_addr,"");
      m_first_tag = true;

    }

    /*
     * Our virtual destructor.
     */
    end_node_zmq_sink_impl::~end_node_zmq_sink_impl()
    {
    }

    int
    end_node_zmq_sink_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex *)input_items[0];

      int nitem_to_process = noutput_items;

      std::vector<tag_t> tags;

      get_tags_in_window(tags, 0, 0, noutput_items, pmt::string_to_symbol("frame_len"));
      if (tags.size())
      {
        if (tags[0].offset != nitems_read(0)) // only process items until next tag
        {
          nitem_to_process = tags[0].offset - nitems_read(0);
        }
        else
        {
          m_frame_length = pmt::to_long(tags[0].value);
          if (m_first_tag) // get time reference
          {
            m_start = std::chrono::steady_clock::now();
            uint64_t microseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            m_offset = microseconds_since_epoch* ((float)m_samp_rate * 1e-6);
            m_first_tag = false;
          }
          m_now = std::chrono::steady_clock::now();
          int64_t nanoseconds_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(m_now - m_start).count();

          m_offset += (nanoseconds_elapsed * ((float)m_samp_rate * 1e-9));//samples since epoch

          zmq::message_t message(sizeof(uint64_t));

          memcpy(message.data(), &m_offset, sizeof(uint64_t));
          m_rrh_socket.send(message, zmq::send_flags::sndmore);
          zmq::message_t message_size(sizeof(long));
          memcpy(message_size.data(), &m_frame_length, sizeof(long));
          m_rrh_socket.send(message_size, zmq::send_flags::sndmore);
          s_sendmore(m_rrh_socket, "");
          // debug_print("New frame with length: " << m_frame_length << ", at idx: " << m_offset,"");
          m_start = std::chrono::steady_clock::now();
        }
      }
      if (m_frame_length - nitem_to_process > 0) // send samples with sndmore flag
      {
        // debug_print("nitem to process: "<<nitem_to_process,"");
        zmq::message_t message(nitem_to_process * sizeof(gr_complex));
        memcpy(message.data(), in, nitem_to_process * sizeof(gr_complex));
        m_rrh_socket.send(message, zmq::send_flags::sndmore);
        m_frame_length -= nitem_to_process;
      }
      else // send end of frame samples
      {

        zmq::message_t message(nitem_to_process * sizeof(gr_complex));
        memcpy(message.data(), in, nitem_to_process * sizeof(gr_complex));
        m_rrh_socket.send(message, zmq::send_flags::none);
        // debug_print("Frame completely sent","");
      }

      // Tell runtime system how many output items we produced.
      return nitem_to_process;
    }

  } /* namespace cran */
} /* namespace gr */

