/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_LORA_SDR_FRAME_SYNC_V2_IMPL_H
#define INCLUDED_LORA_SDR_FRAME_SYNC_V2_IMPL_H

// #define GRLORA_DEBUG
// #define PRINT_DEBUG
#define TIMEOUT 60     ///< Idle Timeout in seconds before clearing buffer
#include <iostream>
#include <fstream>
#include <gnuradio/lora_sdr/frame_sync_v2.h>
#include <gnuradio/lora_sdr/utilities.h>
#include <volk/volk.h>
#include <chrono>
#include <uhd/types/time_spec.hpp>
extern "C"
{
#include "kiss_fft.h"
}

namespace gr
{
    namespace lora_sdr
    {

        class frame_sync_v2_impl : public frame_sync_v2
        {
        private:
            enum DecoderState
            {
                DETECT,
                SYNC,
                SFO_COMPENSATION,
                STOP
            };
            enum SyncState
            {
                NET_ID1,
                NET_ID2,
                DOWNCHIRP1,
                DOWNCHIRP2,
                QUARTER_DOWN
            };

            struct sync_info
            {
                float cfo_frac;
                float sto_frac;
                int cfo_int;
                int sto_int;
                float sfo;
                //float corr_with_ref_pre;
                float noise_power;  // estimated symbol by symbol
                float signal_power; // estimated symbol by symbol
                //float corr_en;
                float upchirps_snr_est; //snr estimated over all upchirps
                std::vector<gr_complex> corrected_preamb;//samples of the corrected preamble Useful for debugging
            };

            char m_identity[10] = {}; //unique identifier

            uint8_t m_state;                    ///< Current state of the synchronization
            uint32_t m_center_freq;             ///< RF center frequency
            uint32_t m_bw;                      ///< Bandwidth
            uint32_t m_samp_rate;               ///< Sampling rate
            uint8_t m_sf;                       ///< Spreading factor
            uint8_t m_cr;                       ///< Coding rate
            uint32_t m_pay_len;                 ///< payload length
            uint8_t m_has_crc;                  ///< CRC presence
            uint8_t m_invalid_header;           ///< invalid header checksum
            bool m_impl_head;                   ///< use implicit header mode
            uint8_t m_os_factor;                ///< oversampling factor
            std::vector<uint16_t> m_sync_words; ///< vector containing the two sync words (network identifiers)
            bool m_ldro;                        ///< use of low datarate optimisation mode
            tag_t m_frame_tag;                  ///< tag with frame information
            bool m_received_frame_tag;          ///< indicate that we received frame informations via a tag
            uint16_t m_n_up;                    ///< number of upchirps in the preamble
            uint8_t m_n_repetitions_req;        ///< number of consecutive repeating values required to trigger a detection
            uint8_t m_n_repetitions;            ///< number of repetitions found
            int m_max_snr_diff;                 ///< Maximum SNR estimated value discrepancy between symbol-by-symbol estimate and full preamble estimate (in dB)
            // uint8_t m_additional_symbols_cnt; ///< counter of additional symbols processed
            uint8_t m_max_additional_symbols; ///< max number of additional symbols with repeating values to be processed

            bool m_found_repetitions;      ///< indicates that we found enough symbol repetitions to consider a preamble
            uint32_t m_bins_per_symbol;    ///< Number of bins in each lora Symbol
            uint32_t m_samples_per_symbol; ///< Number of samples received per lora symbols
            int32_t m_symb_numb;          ///< number of payload lora symbols
            bool m_received_head;          ///< indicate that the header has be decoded and received by this block
            double m_noise_est;            ///< estimate of the noise
            int m_buffer_read_offset;      ///< reading offset in input buffer (to keep some previous samples available)

            int m_n_sync_candidates; ///< number of candidate offset to try to synchronize (each candidate are separate by 2^sf*os_factor samples). Should be odd.

            std::vector<sync_info> m_sync_candidates; /// list of the offset computed for each frame start candidates
            float m_noise_var;                        ///< estimated noise variance
            float m_signal_pow;                       ///< estimated signal power (avg energy per sample)

            std::vector<gr_complex> m_in_down;      ///< downsampled input
            std::vector<gr_complex> m_downchirp;    ///< Reference downchirp
            std::vector<gr_complex> m_upchirp;      ///< Reference upchirp
            std::vector<gr_complex> m_ref_preamble; ///< reference preamble

            std::vector<gr_complex> m_preamb_corr; ///< result of the correlation of the candidate preamble and the reference

            uint frame_cnt;                       ///< Number of frame received
            int32_t symbol_cnt;                   ///< Number of symbols already received
            std::pair<int, float> m_prev_bin_idx; ///< value of previous lora symbol
            int32_t bin_idx_new;                  ///< value of newly demodulated symbol

            int64_t m_last_tag_offset; ///< offset of the last valid tag we received

            uint8_t additional_upchirps; ///< indicate the number of additional upchirps found in preamble (in addition to the minimum required to trigger a detection)

            // int items_to_consume; ///< Number of items to consume after each iteration of the general_work function

            int one_symbol_off;                             ///< indicate that we are offset by one symbol after the preamble
            std::vector<gr_complex> additional_symbol_samp; ///< save the value of the last 1.25 downchirp as it might contain the first payload symbol
            std::vector<gr_complex> preamble_raw;           ///< vector containing the preamble upchirps without any synchronization
            std::vector<gr_complex> preamble_raw_up;        ///< vector containing the upsampled preamble upchirps without any synchronization
            std::vector<gr_complex> downchirp_raw;          ///< vetor containing the preamble downchirps without any synchronization
            std::vector<gr_complex> preamble_upchirps;      ///< vector containing the preamble upchirps
            std::vector<gr_complex> net_id_samp;            ///< vector of the oversampled network identifier samples
            std::vector<int> net_ids;                       ///< values of the network identifiers received

            int up_symb_to_use;                                  ///< number of upchirp symbols to use for CFO and STO frac estimation
            std::vector<std::pair<int, float>> m_preamb_up_vals; ///< value and magnitude of the preamble upchirps

            float m_cfo_frac;                            ///< fractional part of CFO
            float m_cfo_frac_bernier;                    ///< fractional part of CFO using Berniers algo
            int m_cfo_int;                               ///< integer part of CFO
            float m_sto_frac;                            ///< fractional part of CFO
            float m_sfo_symb;                            ///< estimated sampling frequency offset per symbol
            float m_sfo_cum;                             ///< cumulation of the sfo
            bool cfo_frac_sto_frac_est;                  ///< indicate that the estimation of CFO_frac and STO_frac has been performed
            std::vector<gr_complex> CFO_frac_correc;     ///< cfo frac correction vector
            std::vector<gr_complex> CFO_SFO_frac_correc; ///< correction vector accounting for cfo and sfo

            std::vector<gr_complex> symb_corr; ///< symbol with CFO frac corrected
            int down_val;                      ///< value of the preamble downchirps
            int net_id_off;                    ///< offset of the network identifier

            ::uhd::time_spec_t m_current_time; ///< time of the current first sample 
            bool m_is_first_sample; ///< indicate that we are processing the first sample of a frame

            std::chrono::time_point<std::chrono::high_resolution_clock> m_prev_time; ///< keep track of last time we process something to detect timeout

            /**
             *   \brief  Handle the reception of the explicit header information, received from the header_decoder block
             */
            void frame_info_handler(pmt::pmt_t frame_info);
            /**
             *  \brief  Set new SF and adapt all related parameters
             */
            void set_sf(int sf);

            /**
             * @brief dechirp and compute the FFT of the input samples.
             *
             * @param samples The input samples
             * @param ref_chirp The reference chirp to use for dechirping
             * @param length The number of samples used for dechirping and DFT
             * @return std::pair<int,float>, containing the index and magnitude of the maximum bin of the FFT
             */
            std::pair<int, float> dechirp_and_fft(const gr_complex *samples, std::vector<gr_complex> *ref_chirp, uint16_t length);
            int my_roundf(float number);

            float estimate_CFO_frac(gr_complex *samples);
            float estimate_STO_frac(gr_complex *samples);
            /**
             * @brief compute the noise and signal energy, using by dechirphing the signal and taking the FFT
             *
             * @param samples pointer to th start of the symbol samples
             * @param out_noise_pow output the estimated noise power to this variable
             * @param out_signal_pow output the estimated signal power to this variable
             * @param additional_symbol_bin additional bins to consider as signal to mitigate mis-syncronisation (e.g. 1 will use three bins, 2 -> 5 bins)
             * @return float
             */
            float determine_snr(const gr_complex *samples, const gr_complex *ref_downchirp, float *out_noise_pow, float *out_signal_pow, int number_of_samples, int additional_symbol_bin);

        public:
            frame_sync_v2_impl(uint32_t center_freq, uint32_t bandwidth, uint8_t sf,
                               bool impl_head, std::vector<uint16_t> sync_word,
                               uint8_t os_factor, uint16_t preamble_len);
            ~frame_sync_v2_impl();

            // Where all the action really happens
            void forecast(int noutput_items, gr_vector_int &ninput_items_required);

            int general_work(int noutput_items, gr_vector_int &ninput_items,
                             gr_vector_const_void_star &input_items,
                             gr_vector_void_star &output_items);
        };

    } // namespace lora_sdr
} // namespace gr

#endif /* INCLUDED_LORA_SDR_FRAME_SYNC_V2_IMPL_H */
