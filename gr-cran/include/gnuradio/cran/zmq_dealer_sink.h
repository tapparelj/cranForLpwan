/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_ZMQ_DEALER_SINK_H
#define INCLUDED_CRAN_ZMQ_DEALER_SINK_H

#include <gnuradio/cran/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace cran {

/*!
 * \brief <+description of block+>
 * \ingroup cran
 *
 */
class CRAN_API zmq_dealer_sink : virtual public gr::sync_block {
public:
  typedef std::shared_ptr<zmq_dealer_sink> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of cran::zmq_dealer_sink.
   *
   * To avoid accidental use of raw pointers, cran::zmq_dealer_sink's
   * constructor is in a private implementation
   * class. cran::zmq_dealer_sink::make is the public interface for
   * creating new instances.
   */
  static sptr make();
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_ZMQ_DEALER_SINK_H */
