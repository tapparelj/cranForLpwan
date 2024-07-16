/* -*- c++ -*- */
/* 
 * Copyright 2022 Tapparel Joachim @EPFL,TCL.
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


#ifndef INCLUDED_LORA_SDR_HAMMING_DEC_H
#define INCLUDED_LORA_SDR_HAMMING_DEC_H

#include <gnuradio/lora_sdr/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace lora_sdr {

    /*!
     * \brief <+description of block+>
     * \ingroup lora_sdr
     *
     */
    class LORA_SDR_API hamming_dec : virtual public gr::sync_block
    {
     public:
      typedef std::shared_ptr<hamming_dec> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of lora_sdr::hamming_dec.
       *
       * To avoid accidental use of raw pointers, lora_sdr::hamming_dec's
       * constructor is in a private implementation
       * class. lora_sdr::hamming_dec::make is the public interface for
       * creating new instances.
       */
      static sptr make(bool soft_decoding);
    };

  } // namespace lora_sdr
} // namespace gr

#endif /* INCLUDED_LORA_SDR_HAMMING_DEC_H */

