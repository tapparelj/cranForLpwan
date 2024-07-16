/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_RX_COMBINING_H
#define INCLUDED_CRAN_RX_COMBINING_H

#include <gnuradio/block.h>
#include <gnuradio/cran/api.h>

namespace gr {
namespace cran {

/*!
 * \brief <+description of block+>
 * \ingroup cran
 *
 */
class CRAN_API rx_combining : virtual public gr::block {
public:
  typedef std::shared_ptr<rx_combining> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of cran::rx_combining.
   *
   * To avoid accidental use of raw pointers, cran::rx_combining's
   * constructor is in a private implementation
   * class. cran::rx_combining::make is the public interface for
   * creating new instances.
   */
  static sptr make(std::string input_addr);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_RX_COMBINING_H */
