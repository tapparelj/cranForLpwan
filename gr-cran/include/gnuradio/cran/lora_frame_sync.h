/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_LORA_FRAME_SYNC_H
#define INCLUDED_CRAN_LORA_FRAME_SYNC_H

#include <gnuradio/block.h>
#include <gnuradio/cran/api.h>

namespace gr {
namespace cran {

/*!
 * \brief <+description of block+>
 * \ingroup cran
 *
 */
class CRAN_API lora_frame_sync : virtual public gr::block {
public:
  typedef std::shared_ptr<lora_frame_sync> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of cran::lora_frame_sync.
   *
   * To avoid accidental use of raw pointers, cran::lora_frame_sync's
   * constructor is in a private implementation
   * class. cran::lora_frame_sync::make is the public interface for
   * creating new instances.
   */
  static sptr make(uint32_t center_freq, uint32_t bandwidth, uint8_t sf,
                   bool impl_head, std::vector<uint16_t> sync_word,
                   uint8_t os_factor, uint16_t preamble_len = 8);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_LORA_FRAME_SYNC_H */
