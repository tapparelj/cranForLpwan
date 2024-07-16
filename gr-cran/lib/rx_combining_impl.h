/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_CRAN_RX_COMBINING_IMPL_H
#define INCLUDED_CRAN_RX_COMBINING_IMPL_H

#include <string>
#include <iostream>
#include <zmq.hpp>
#include <gnuradio/cran/rx_combining.h>
#include "gnuradio/cran/zhelper.h"
#include "gnuradio/cran/utilities.h"
#include <boost/math/special_functions/bessel.hpp>
#include <fstream>
#include <filesystem>
extern "C"
{
#include "kiss_fft.h"
}

#define GR_HEADER_MAGIC 0x5FF0 //from gnuradio
#define GR_HEADER_VERSION 0x01 //from gnuradio
#define LLR_CLAMPING_VALUE 100.0
#define RRH_WAIT_MS 100 ///< delay after the first RRH connects that we wait for additionnal RRHs (in milliseconds) 
#define MAX_COMBINED_RRH 20
#define LDRO_AUTO_MAX_DURATION_MS 16 ///<max duration of a symbol before it uses LDRO when in AUTO mode

namespace gr
{
    namespace cran
    {
        struct rrh_param
        {
            std::string rrh_id;
            float cfo;
            float noise_pow;
            float sig_pow;
            std::vector<gr_complex> ref_downchirp;
            std::vector<gr_complex> rx_samples;
            std::vector<float> symb_prob; //only used for approximated combining
            std::vector<float> rice_val; // only used for non approximated combining
            std::vector<float> rayleigh_val; // only used for non approximated combining
            uint64_t time_full; //time of the first sample of data received from the RRH
            double time_frac;
            std::string buffer_addr;
        };

        class rx_combining_impl : public rx_combining
        {
        private:
            char m_identity[10] = {}; //unique identifier

            bool m_use_approx_comb = true; ///< use the approximated combining method

            // ZMQ related
            zmq::context_t m_context{1};
            zmq::socket_t m_input_socket{m_context, zmq::socket_type::router};
            zmq::pollitem_t m_pollitems[1];
            zmq::message_t m_msg_buff;
            std::string m_sync_worker_addr;
            size_t m_consumed_bytes;
            std::vector<gr::tag_t> m_tags; ///< input tags received from the RRHs
            gr::tag_t m_frame_tag;         ///< tag output after the combination
            std::vector<gr_complex> rx_samples;

            uint16_t m_work_id=0;

            bool m_waiting; ///< indicate that we wait for other RRH streams to arrive

            uint8_t m_sf;                        ///< Spreading factor
            uint64_t m_bw;                       ///< LoRa bandwidth used
            bool m_ldro;                         ///< use low data-rate optimisation
            uint32_t m_samples_per_symbol;       ///< Number of samples received per lora symbols
            bool m_is_header;                    ///< Indicate that the first block hasn't been fully received
            std::vector<float> m_fft_mag;        ///< Result of the |FFT|
            std::vector<gr_complex> m_upchirp;   ///< Reference upchirp
            std::vector<gr_complex> m_downchirp; ///< Reference downchirp
            std::vector<gr_complex> m_dechirped; ///< Dechirped symbol
            uint m_output_symbol_count;            ///< number of symbols llrs output
            bool m_decoded_header;               ///< indicate that header was decoded and we can continue process the rest of the frame
            int m_symb_numb;                     ///< number of symobl in frame based on received header
            uint numb_processed_symb = 0; ///< number of symbols we processed

            bool m_work_done;
            std::chrono::time_point<std::chrono::steady_clock> m_first_rrh_time; ///< time that we received samples from the first rrh
            bool m_first_rrh_time_received = false; ///< indicate that the time has be set by the arrival of a first RRH stream

            std::vector<float> m_rice_val;     ///< output of compute_rice
            std::vector<float> m_rayleigh_val; ///< output of compute_rayleigh
            std::vector<float> m_tmp_prob;     ///< temp vector for computing symbol probabilities

            std::vector<rrh_param> m_rrh_params; ///< parameters of all currently processed RRHs
            bool m_is_new_stream;
            bool m_force_long_wait; ///< for to wait for thenext message after receiving the decoded header information

            // std::ofstream log_time_file;

            bool once_per_stream = true;// used to print bessel overflow only once per received stream         


            /**
             * @brief Set spreading factor and init vector sizes accordingly
             *
             */
            void set_sf(int sf);
            void frame_info_handler(pmt::pmt_t frame_info);

            void compute_dechirp_fft_mag(const gr_complex *samples, gr_complex *ref_downchirp, float *out_fft_mag);
            bool compute_rice(const float *bins, float *output, int size, float signal_power, float noise_var); // retrns true if the output is valid
            bool compute_scaled_rayleigh(const float *bins, float *output, int size, float noise_var, float scaling_factor);// retrns true if the output is valid
            void free_worker_for_next_work(std::string msg);

            std::vector<double> compute_llrs(const float *samples, int size, bool use_ldro);

        public:

            rx_combining_impl(std::string input_addr);
            ~rx_combining_impl();

            int general_work(int noutput_items, gr_vector_int &ninput_items,
                             gr_vector_const_void_star &input_items,
                             gr_vector_void_star &output_items);
        };
    } // namespace cran
} // namespace gr

#endif /* INCLUDED_CRAN_RX_COMBINING_IMPL_H */
