/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_WORKER_ZMQ_SINK_H
#define INCLUDED_CRAN_WORKER_ZMQ_SINK_H

#include <gnuradio/cran/api.h>
#include <gnuradio/sync_block.h>


namespace gr {
namespace cran {

/*!
 * \brief <+description of block+>
 * \ingroup cran
 *
 */
class CRAN_API worker_zmq_sink : virtual public gr::sync_block {
public:
  typedef std::shared_ptr<worker_zmq_sink> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of cran::worker_zmq_sink.
   *
   * To avoid accidental use of raw pointers, cran::worker_zmq_sink's
   * constructor is in a private implementation
   * class. cran::worker_zmq_sink::make is the public interface for
   * creating new instances.
   */
  static sptr make(std::string broker_addr, std::string application_addr);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_WORKER_ZMQ_SINK_H */
