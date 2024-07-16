/* -*- c++ -*- */
/*
 * Copyright 2023 Joachim Tapparel@ EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lora_frame_detector_single_sf_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace cran {

using input_type = gr_complex;
using output_type = gr_complex;
lora_frame_detector_single_sf::sptr
lora_frame_detector_single_sf::make(int samp_rate, int sf) {
  return gnuradio::make_block_sptr<lora_frame_detector_single_sf_impl>(
      samp_rate, sf);
}

/*
 * The private constructor
 */
lora_frame_detector_single_sf_impl::lora_frame_detector_single_sf_impl(
    int samp_rate, int sf): gr::block("lora_frame_detector_single_sf",gr::io_signature::make(1 , 1 ,sizeof(input_type)),gr::io_signature::make(1 , 1 ,sizeof(output_type))) 
{
  m_samp_rate = samp_rate;
  m_bandwidth = 125000; //TODO add this as a parameter
  m_sf = sf;
  warning_print("only SF"<<m_sf<<" is considered for detection!!","");
  
  m_n_symb_in_window = 7;
  m_samples_per_symbol = (1u << m_sf)*m_samp_rate/m_bandwidth;
  m_bins_per_symbol = (1u << m_sf);
  m_buffer_offset = m_samples_per_symbol*10;
  m_samples_per_window = m_n_symb_in_window * m_samples_per_symbol;
  m_n_samples_to_advance_each_loop = int((float)m_samples_per_symbol/2);
  
  m_resyc_sample_period = m_samp_rate*60;//every minute
  m_last_resync = 0;

  // --- Downchirps Calculation ---
  // Temporary vector to save the downchirps for the considered SF
  m_downchirps.resize(m_samples_per_window);

  // Calculate the downchirp
  for (int n = 0; n < m_samples_per_window; n++)
  {
      m_downchirps[n] = gr_expj(-2.0 * M_PI * (pow(n, 2) / (1u << (m_sf+1)) - 0.5 * n));
  }

  set_output_multiple(m_n_samples_to_advance_each_loop);
  m_dechirped.resize(m_samples_per_window, 0.0);

  // initialise FFTW
  m_in_fft = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * m_samples_per_window);
  m_out_fft = (fftwf_complex*)fftw_malloc(sizeof(fftwf_complex) * m_samples_per_window);
  
  m_fft_plan = fftwf_plan_dft_1d(m_samples_per_window, m_in_fft, m_out_fft, FFTW_FORWARD, FFTW_ESTIMATE);

  // idx_buffer = 0;
  // nr_samples = 0;
  // tot_samples = 0;  

  // detected = false;
  //m_noise_power = 2;
  m_got_first_pps_time = false;    
  srand(time(NULL)); 
  m_id = std::rand() % 100;


}
/*
 * Our virtual destructor.
 */
lora_frame_detector_single_sf_impl::~lora_frame_detector_single_sf_impl() {}

void lora_frame_detector_single_sf_impl::forecast(
  int noutput_items, gr_vector_int &ninput_items_required) {
  ninput_items_required[0] = m_samples_per_window + m_buffer_offset; 
}

void lora_frame_detector_single_sf_impl::new_tag(uint64_t tag_offset, int sf)
{
    pmt::pmt_t user_tag = pmt::make_dict();

    //   user_tag = pmt::dict_add(user_tag, pmt::intern("sf"), pmt::from_long(sf_vec.back()));
    user_tag = pmt::dict_add(user_tag, pmt::intern("sf"), pmt::from_long(sf));
    //get the real time of the sample we attach the tag
    auto sample_abs_time = m_current_time;

    user_tag = pmt::dict_add(user_tag, pmt::intern("full_secs"), pmt::from_uint64(sample_abs_time.get_full_secs()));
    user_tag = pmt::dict_add(user_tag, pmt::intern("frac_secs"), pmt::from_double(sample_abs_time.get_frac_secs()));

    user_tag = pmt::dict_add(user_tag, pmt::intern("begin"), pmt::from_bool(true));
    add_item_tag(0, tag_offset, pmt::string_to_symbol("frame_info"), user_tag);
}

float lora_frame_detector_single_sf_impl::compute_decision_metric(const gr_complex* samples, int sf, int os_factor, int n_symb)
{
  int samp_per_window = n_symb*(1u<<sf);
  //decimate data
  if(m_decim_samples.size()!=samp_per_window){
    m_decim_samples.resize(n_symb*(1u<<sf));
  }
  for(int i=0; i<samp_per_window; i++){
    m_decim_samples[i] = samples[i*os_factor];
  }
  

  volk_32fc_x2_multiply_32fc(m_dechirped.data(), m_decim_samples.data(), m_downchirps.data(), samp_per_window);

  kiss_fft_cfg cfg = kiss_fft_alloc(samp_per_window, 0, 0, 0);
  kiss_fft_cpx *cx_in = new kiss_fft_cpx[samp_per_window];
  kiss_fft_cpx *cx_out = new kiss_fft_cpx[samp_per_window];

  for(int i=0;i<samp_per_window;i++)
  {
    cx_in[i].r = m_dechirped[i].real();
    cx_in[i].i = m_dechirped[i].imag();
  }
  // do the FFT
  kiss_fft(cfg, cx_in, cx_out);

  // fftwf_execute_dft(m_fft_plan, m_in_fft, m_out_fft);

  // Calculate Magnitude
  std::vector<float> mag_sq(samp_per_window,0);

  float energy_tot = 0;
  for (int i = 0u; i < samp_per_window; i++)
  {
      mag_sq[i] = cx_out[i].r*cx_out[i].r + cx_out[i].i*cx_out[i].i;
      energy_tot += mag_sq[i];
      
  }

  free(cfg);
  delete[] cx_in;
  delete[] cx_out;
  // Max bin 
  float m_max = *max_element(mag_sq.begin(), mag_sq.end());

  // Check thresholds of metric defined in SPAWC23
  // compute SNR estimate
  float noise_pow_est = (energy_tot - m_max );
  float SNR_est = 10 * log10(m_max  / noise_pow_est);
  return SNR_est;
}

::uhd::time_spec_t lora_frame_detector_single_sf_impl::get_current_time()
{
    auto now = std::chrono::system_clock::now();
    uint64_t integer_time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    uint64_t nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    uint64_t frac_time_int = nanoseconds - integer_time*1E9;
    double frac_time = (double)frac_time_int/1E9;
    std::cout<<"current_time: "<<std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()<<" = "<<integer_time<<" "<<frac_time<<std::endl;
    
    ::uhd::time_spec_t current_time = ::uhd::time_spec_t(integer_time,frac_time);
    return current_time;
  }

int lora_frame_detector_single_sf_impl::general_work(
    int noutput_items, gr_vector_int &ninput_items,
    gr_vector_const_void_star &input_items, gr_vector_void_star &output_items) {
  auto in = static_cast<const input_type *>(input_items[0]);
  auto out = static_cast<output_type *>(output_items[0]);

  int to_output = 0;
  int to_consume = 0;

  // Look for initial time tag
  std::vector<tag_t> tags;
  get_tags_in_window(tags, 0, 0, m_n_samples_to_advance_each_loop, pmt::string_to_symbol("rx_time"));
  if (tags.size()){
    uint64_t integer_time;
    double frac_time;
    integer_time = pmt::to_uint64(pmt::tuple_ref(tags[0].value,0));
    frac_time = pmt::to_double(pmt::tuple_ref(tags[0].value,1));
    //debug_print("Got absolute time tag from sdr: offset="<<tags[0].offset<< ",val=" <<integer_time<<" "<<frac_time, m_id, "frame_detector_single_sf", WHITE);
    // if time ==0 -> SDR don't provide absolute time
    if(integer_time<1700000000) //we dont get a correct unix time
    {// get current time
      m_current_time = get_current_time();
    }
    else{
      m_current_time = ::uhd::time_spec_t(integer_time,frac_time) + (double(nitems_read(0))-tags[0].offset )/(float)m_samp_rate;
    }
      
  }
  //LimeSDr based RRH have no PPS input
  //Look for time last pps tag to correct drift
  get_tags_in_window(tags, 0, 0, m_n_samples_to_advance_each_loop, pmt::string_to_symbol("time_last_pps"));
  if (tags.size()){
    uint64_t integer_time;
    double frac_time;
    integer_time = pmt::to_uint64(pmt::tuple_ref(tags[0].value,0));
    frac_time = pmt::to_double(pmt::tuple_ref(tags[0].value,1));
    
    //get drift from the expected 1 second between pps:
    ::uhd::time_spec_t time_last_pps_new = ::uhd::time_spec_t(integer_time,frac_time);

    if(m_got_first_pps_time){//only correct offset when two pps time have been received
      auto time_offset = time_last_pps_new - m_time_last_pps;
      // debug_print("time_last_pps_new="<<time_last_pps_new.get_full_secs()<< " " <<time_last_pps_new.get_frac_secs() , m_id, "frame_detector_single_sf", WHITE);
      // debug_print("m_time_last_pps="<<m_time_last_pps.get_full_secs()<< " " <<m_time_last_pps.get_frac_secs() , m_id, "frame_detector_single_sf", WHITE);


      if(time_offset.get_real_secs() > 5){
          error_print("Missed 5 pps, there is probably an issue with the GPS lock!","");
      }
      time_offset = time_offset - std::round(time_offset.get_real_secs());
      //debug_print("got time last pps usrp: time offset="<<time_offset.get_real_secs()<< ",val=" <<integer_time<<" "<<frac_time, m_id, "frame_detector_single_sf", WHITE);

      m_current_time -= time_offset;
      m_last_resync = nitems_read(0) + ninput_items[0];
    }
    else{
        //debug_print("time_last_pps_new first time ="<<time_last_pps_new.get_full_secs()<< " " <<time_last_pps_new.get_frac_secs(), m_id, "frame_detector_single_sf", WHITE);
        m_got_first_pps_time = true;
    }
    m_time_last_pps = time_last_pps_new;
  }
  
  //if enough samples passed, sync time using clock
  if(nitems_read(0)+ninput_items[0]-m_last_resync > m_resyc_sample_period){
    m_current_time = get_current_time();
    warning_print("We had to resync using local clock after "<<m_resyc_sample_period/m_samp_rate<<" s since last PPS synchro " << m_current_time.get_full_secs()<<" "<< m_current_time.get_frac_secs(),m_id);
    m_last_resync = nitems_read(0) + ninput_items[0];
  }

  // Dechirp
  
  float metric_val = compute_decision_metric(&in[m_buffer_offset],m_sf, m_samp_rate/m_bandwidth, m_n_symb_in_window);

  // if(metric_val > m_thresholded_val[m_sf-7]-2) 
  // {
  //   // m_debugfile<<m_current_time.get_full_secs()<<","<<m_current_time.get_frac_secs() <<","<<metric_val<<std::endl;
  // }
  
 //check if detection metric above threshold
 if(metric_val > m_thresholded_val[m_sf-7])//7 is the minimum SF we consider
  {
    // std::cout<<GREEN<<"metric: "<<metric_val<<RESET<<std::endl;
    int sample_preamb = nitems_written(0);
    new_tag(sample_preamb, m_sf);
  }
  else{
    // std::cout<<"metric: "<<metric_val<<std::endl;
  }
  memcpy(&out[0], &in[0], m_n_samples_to_advance_each_loop * sizeof(gr_complex));


  to_output = m_n_samples_to_advance_each_loop;
  to_consume = m_n_samples_to_advance_each_loop;

  // Tell runtime system how many input items we consumed on
  // each input stream.
  consume_each(to_consume);
  m_current_time += double(to_consume)/m_samp_rate;

  // Tell runtime system how many output items we produced.
  return to_output;
}

} /* namespace cran */
} /* namespace gr */
