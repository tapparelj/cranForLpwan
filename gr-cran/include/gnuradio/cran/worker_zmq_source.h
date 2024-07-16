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

#ifndef INCLUDED_CRAN_WORKER_ZMQ_SOURCE_H
#define INCLUDED_CRAN_WORKER_ZMQ_SOURCE_H

#include <gnuradio/cran/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace cran {

    /*!
     * \brief <+description of block+>
     * \ingroup cran
     *
     */
    class CRAN_API worker_zmq_source : virtual public gr::sync_block
    {
     public:
      typedef std::shared_ptr<worker_zmq_source> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of cran::worker_zmq_source.
       *
       * To avoid accidental use of raw pointers, cran::worker_zmq_source's
       * constructor is in a private implementation
       * class. cran::worker_zmq_source::make is the public interface for
       * creating new instances.
       */
      static sptr make(std::string broker_addr);
    };

  } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_WORKER_ZMQ_SOURCE_H */

