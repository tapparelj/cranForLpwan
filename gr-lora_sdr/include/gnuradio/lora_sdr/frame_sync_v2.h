/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_LORA_SDR_FRAME_SYNC_V2_H
#define INCLUDED_LORA_SDR_FRAME_SYNC_V2_H

#include <gnuradio/block.h>
#include <gnuradio/lora_sdr/api.h>

namespace gr {
namespace lora_sdr {

/*!
 * \brief <+description of block+>
 * \ingroup lora_sdr
 *
 */
class LORA_SDR_API frame_sync_v2 : virtual public gr::block {
public:
  typedef std::shared_ptr<frame_sync_v2> sptr;

  /*!
   * \brief Return a shared_ptr to a new instance of lora_sdr::frame_sync_v2.
   *
   * To avoid accidental use of raw pointers, lora_sdr::frame_sync_v2's
   * constructor is in a private implementation
   * class. lora_sdr::frame_sync_v2::make is the public interface for
   * creating new instances.
   */
  static sptr make(uint32_t center_freq, uint32_t bandwidth, uint8_t sf,
                   bool impl_head, std::vector<uint16_t> sync_word,
                   uint8_t os_factor, uint16_t preamble_len = 8);
};

} // namespace lora_sdr
} // namespace gr

#endif /* INCLUDED_LORA_SDR_FRAME_SYNC_V2_H */
