/* -*- c++ -*- */
/*
 * Copyright 2023 Joachim Tapparel@ EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_H
#define INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_H

#include <gnuradio/block.h>
#include <gnuradio/cran/api.h>


namespace gr {
namespace cran {

/*!
 * \brief <+description of block+>
 * \ingroup cran
 *
 */
class CRAN_API lora_frame_detector_single_sf : virtual public gr::block {
public:
  typedef std::shared_ptr<lora_frame_detector_single_sf> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of
   * cran::lora_frame_detector_single_sf.
   *
   * To avoid accidental use of raw pointers,
   * cran::lora_frame_detector_single_sf's constructor is in a private
   * implementation class. cran::lora_frame_detector_single_sf::make is the
   * public interface for creating new instances.
   */
  static sptr make(int samp_rate, int sf);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_H */
