/* -*- c++ -*- */
/*
 * Copyright 2023 Joachim Tapparel@ EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_IMPL_H
#define INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_IMPL_H

// Macros for real and imaginary part
#define REAL 0
#define IMAG 1


#include <gnuradio/cran/lora_frame_detector_single_sf.h>
#include "gnuradio/cran/utilities.h"
#include <fftw3.h>          // FFT library
#include <fstream>          // Library for saving to file

#include <uhd/types/time_spec.hpp>
extern "C"
{
#include "kiss_fft.h"
}

namespace gr {
namespace cran {

class lora_frame_detector_single_sf_impl
    : public lora_frame_detector_single_sf {
private:
  int m_sf;
  int m_samp_rate;
  int m_bandwidth;
  int m_samples_per_symbol;
  int m_bins_per_symbol;
  int m_samples_per_window;
  int m_n_symb_in_window;
  std::vector<gr_complex> m_downchirps;        // Vector to store downchirps
  std::vector<gr_complex> m_dechirped;                     // Vector to store dechirped symbol
  std::vector<gr_complex> m_decim_samples;  
  float m_noise_power;
  int m_n_samples_to_advance_each_loop; ///< the number of samples that we process each work loop
  std::vector<float> m_thresholded_val = {-16.5,-19.5,-22.5,-25.5,-28.5,-31.5};//for 7 symbols, 1 FP per 3600s

  int m_buffer_offset; // reading offset in the input buffer
  
  //FFTW 
  fftwf_plan m_fft_plan;
  fftwf_complex* m_in_fft;
  fftwf_complex* m_out_fft;
  bool plot_once =true;

  // Time related
  ::uhd::time_spec_t m_current_time; ///<current time of the first sample in input buffer
  bool m_got_first_pps_time;
  ::uhd::time_spec_t m_time_last_pps;
  uint64_t m_resyc_sample_period; ///< The period in number of samples to ask the system for the time
  uint64_t m_last_resync;

  //debugging
  int m_id;
  std::ofstream m_debugfile;

void new_tag(uint64_t tag_offset, int sf);
float compute_decision_metric(const gr_complex *in_fft, int sf, int os_factor, int n_symb);
::uhd::time_spec_t get_current_time();

public:
  lora_frame_detector_single_sf_impl(int samp_rate, int sf);
  ~lora_frame_detector_single_sf_impl();

  // Where all the action really happens
  void forecast(int noutput_items, gr_vector_int &ninput_items_required);

  int general_work(int noutput_items, gr_vector_int &ninput_items,
                   gr_vector_const_void_star &input_items,
                   gr_vector_void_star &output_items);
};

} // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_LORA_FRAME_DETECTOR_SINGLE_SF_IMPL_H */
