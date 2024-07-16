/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lora_frame_sync_impl.h"
#include <gnuradio/io_signature.h>

namespace gr
{
    namespace cran
    {

        using input_type = gr_complex;
        using output_type = gr_complex;
        lora_frame_sync::sptr
        lora_frame_sync::make(uint32_t center_freq, uint32_t bandwidth, uint8_t sf,
                              bool impl_head, std::vector<uint16_t> sync_word,
                              uint8_t os_factor, uint16_t preamble_len)
        {
            return gnuradio::make_block_sptr<lora_frame_sync_impl>(center_freq, bandwidth,
                                                                   sf, impl_head, sync_word,
                                                                   os_factor, preamble_len);
        }

        /*
         * The private constructor
         */
        lora_frame_sync_impl::lora_frame_sync_impl(uint32_t center_freq, uint32_t bandwidth,
                                                   uint8_t sf, bool impl_head,
                                                   std::vector<uint16_t> sync_word,
                                                   uint8_t os_factor, uint16_t preamble_len)
            : gr::block("lora_frame_sync",
                        gr::io_signature::make(1 /* min inputs */, 1 /* max inputs */,
                                               sizeof(input_type)),
                        gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */,
                                               sizeof(output_type)))
        {
            sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));

            m_state = DETECT;
            m_found_repetitions = false;
            m_received_frame_tag = false;
            m_center_freq = center_freq;
            m_bw = bandwidth;
            m_impl_head = impl_head;
            m_os_factor = os_factor;
            m_sync_words = sync_word;
            m_n_up = preamble_len;

            if (preamble_len < 5)
            {
                std::cerr << RED << " Preamble length should be greater than 5!" << RESET << std::endl;
            }

            net_ids.resize(2, 0);

            m_n_repetitions_req = 6; // define this properly
            // up_symb_to_use = m_n_repetitions_req - 1;
            m_max_additional_symbols = 9;
            m_n_sync_candidates = 5;
            m_sync_candidates.resize(m_n_sync_candidates);

            m_buffer_read_offset = (m_n_repetitions_req + m_max_additional_symbols + 1) * m_os_factor * (1u << MAX_SF) + m_os_factor;

            m_sto_frac = 0.0;

            // Convert given sync word into the two modulated values in preamble
            if (m_sync_words.size() == 1)
            {
                uint16_t tmp = m_sync_words[0];
                m_sync_words.resize(2, 0);
                m_sync_words[0] = ((tmp & 0xF0) >> 4) << 3;
                m_sync_words[1] = (tmp & 0x0F) << 3;
            }

            set_sf(sf);
            m_prev_bin_idx = std::pair<int, float>(0, 0);
            m_n_repetitions = 1;
            // m_preamb_up_vals.resize(m_n_repetitions_req + m_max_additional_symbols);
            frame_cnt = 0;
            m_symb_numb = 0; // value for continuous stream after finding a frame

            // m_frame_tag.value = pmt::make_dict();
            // m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("sf"), pmt::mp((long)m_sf));

            set_tag_propagation_policy(TPP_DONT);
            // register message ports
            message_port_register_in(pmt::mp("frame_info"));
            set_msg_handler(pmt::mp("frame_info"), [this](pmt::pmt_t msg)
                            { this->frame_info_handler(msg); });

            message_port_register_out(pmt::mp("preamb_info"));
            

            m_req_item_forecast = m_buffer_read_offset + m_n_up * m_os_factor * (1u << MAX_SF);
        }

        /*
         * Our virtual destructor.
         */
        lora_frame_sync_impl::~lora_frame_sync_impl() {}

        void lora_frame_sync_impl::forecast(int noutput_items,
                                            gr_vector_int &ninput_items_required)
        {
            ninput_items_required[0] = m_req_item_forecast;
            // ninput_items_required[0] = 1;
        }
        void lora_frame_sync_impl::set_sf(int sf)
        {
            m_sf = sf;
            m_bins_per_symbol = (uint32_t)(1u << m_sf);
            m_samples_per_symbol = m_bins_per_symbol * m_os_factor;
            m_max_snr_diff = 3; 
            additional_symbol_samp.resize(2 * m_samples_per_symbol);
            m_upchirp.resize(m_bins_per_symbol);
            m_downchirp.resize(m_bins_per_symbol);
            preamble_upchirps.resize(m_n_up * m_bins_per_symbol);
            preamble_raw_up.resize((m_n_up + 3) * m_samples_per_symbol);
            CFO_frac_correc.resize(m_bins_per_symbol);
            CFO_SFO_frac_correc.resize(m_bins_per_symbol);
            symb_corr.resize(m_bins_per_symbol);
            m_in_down.resize(m_bins_per_symbol * m_n_up);
            preamble_raw.resize(m_n_up * m_bins_per_symbol);
            net_id_samp.resize(m_samples_per_symbol * 2.5); // we should be able to move up to one quarter of symbol in each direction
            lora_sdr::build_ref_chirps(&m_upchirp[0], &m_downchirp[0], m_sf);
            set_output_multiple(m_bins_per_symbol);

            //fft related
            free(m_cfg);
            m_cfg = kiss_fft_alloc(m_n_up * m_bins_per_symbol, 0, 0, 0);

            delete[] m_fft_in;
            delete[] m_fft_out;
            

            m_fft_in = new kiss_fft_cpx[m_n_up * m_bins_per_symbol];
            m_fft_out = new kiss_fft_cpx[m_n_up * m_bins_per_symbol];

            // Buffer has fixed size for multi sf support
            // m_buffer_read_offset = (m_n_repetitions_req + m_max_additional_symbols + 1) * m_samples_per_symbol + m_os_factor;

            // construct ref preamble
            m_ref_preamble.resize(int((m_n_up + 2 + 2.25) * m_bins_per_symbol));
            for (int ii = 0; ii < m_n_up; ii++)
            {
                memcpy(&m_ref_preamble[ii * m_bins_per_symbol], m_upchirp.data(), m_bins_per_symbol * sizeof(gr_complex));
            }
            // sync words
            lora_sdr::build_upchirp(&m_ref_preamble[m_n_up * m_bins_per_symbol], m_sync_words[0], m_sf);
            lora_sdr::build_upchirp(&m_ref_preamble[(m_n_up + 1) * m_bins_per_symbol], m_sync_words[1], m_sf);
            // downchirps
            memcpy(&m_ref_preamble[(m_n_up + 2) * m_bins_per_symbol], m_downchirp.data(), m_bins_per_symbol * sizeof(gr_complex));
            memcpy(&m_ref_preamble[(m_n_up + 3) * m_bins_per_symbol], m_downchirp.data(), m_bins_per_symbol * sizeof(gr_complex));
            memcpy(&m_ref_preamble[(m_n_up + 4) * m_bins_per_symbol], m_downchirp.data(), (m_bins_per_symbol * .25) * sizeof(gr_complex));
            m_prev_time = std::chrono::high_resolution_clock::now();
        }

        void lora_frame_sync_impl::frame_info_handler(pmt::pmt_t frame_info)
        {
            pmt::pmt_t err = pmt::string_to_symbol("error");

            m_cr = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("cr"), err));
            m_pay_len = pmt::to_double(pmt::dict_ref(frame_info, pmt::string_to_symbol("pay_len"), err));
            m_has_crc = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("crc"), err));
            uint8_t ldro_mode = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("ldro_mode"), err));
            m_invalid_header = pmt::to_double(pmt::dict_ref(frame_info, pmt::string_to_symbol("err"), err));

            if (m_invalid_header)
            {
                m_state = DETECT;
                m_sto_frac = 0;
            }
            else
            {
                if (ldro_mode == lora_sdr::AUTO)
                    m_ldro = (float)(1u << m_sf) * 1e3 / m_bw > LDRO_MAX_DURATION_MS;
                else
                    m_ldro = ldro_mode;

                m_symb_numb = 8 + ceil((double)(2 * m_pay_len - m_sf + 2 + !m_impl_head * 5 + m_has_crc * 4) / (m_sf - 2 * m_ldro)) * (4 + m_cr);
                m_received_head = true;

                frame_info = pmt::dict_add(frame_info, pmt::intern("is_header"), pmt::from_bool(false));
                frame_info = pmt::dict_add(frame_info, pmt::intern("symb_numb"), pmt::from_long(m_symb_numb));
                frame_info = pmt::dict_delete(frame_info, pmt::intern("ldro_mode"));

                frame_info = pmt::dict_add(frame_info, pmt::intern("ldro"), pmt::from_bool(m_ldro));

                add_item_tag(0, nitems_written(0), pmt::string_to_symbol("frame_info"), frame_info);
            }
            debug_print("[Time] got frame info at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","lora_frame_sync",GREEN);
        }

        std::pair<int, float> lora_frame_sync_impl::dechirp_and_fft(const gr_complex *samples, std::vector<gr_complex> *ref_chirp, uint16_t length)
        {
            // dechirp
            std::vector<gr_complex> dechirped(length);
            std::vector<gr_complex> extended_downchirps(length);

            for (int ii = 0; ii < length / ref_chirp->size(); ii++)
            {
                memcpy(&extended_downchirps[ii * ref_chirp->size()], ref_chirp->data(), ref_chirp->size() * sizeof(gr_complex));
            }
            volk_32fc_x2_multiply_32fc(&dechirped[0], &samples[0], extended_downchirps.data(), length);

            // do FFT
            for (int i = 0; i < length; i++)
            {
                m_fft_in[i].r = dechirped[i].real();
                m_fft_in[i].i = dechirped[i].imag();
            }
            // do the FFT
            kiss_fft(m_cfg, m_fft_in, m_fft_out);

            // Get magnitude
            float fft_mag_sq[length];

            for (uint32_t i = 0u; i < length; i++)
            {
                fft_mag_sq[i] = m_fft_out[i].r * m_fft_out[i].r + m_fft_out[i].i * m_fft_out[i].i;
            }

            // Return argmax and max here
            auto max_it = std::max_element(fft_mag_sq, fft_mag_sq + length);
            // std::cout << "val idx : " << float(max_it - fft_mag_sq) << " " << std::sqrt(*max_it) << std::endl;
            // get symbol value
            int symb_val = lora_sdr::mod(std::round(float(max_it - fft_mag_sq) * ref_chirp->size() / length), ref_chirp->size());

            return std::pair<int, float>{symb_val, std::sqrt(*max_it)};
        }

        int lora_frame_sync_impl::my_roundf(float number)
        {
            int ret_val = (int)(number > 0 ? int(number + 0.5) : std::ceil(number - 0.5));
            return ret_val;
        }

        float lora_frame_sync_impl::determine_snr(const gr_complex *samples, const gr_complex *ref_downchirp, float *out_noise_pow, float *out_signal_pow, int number_of_samples, int additional_symbol_bin)
        {
            double tot_en = 0;
            float fft_mag_sq[number_of_samples];
            std::vector<gr_complex> dechirped(number_of_samples);

            kiss_fft_cfg cfg = kiss_fft_alloc(number_of_samples, 0, 0, 0);
            kiss_fft_cpx *fft_in = new kiss_fft_cpx[number_of_samples];
            kiss_fft_cpx *fft_out = new kiss_fft_cpx[number_of_samples];
            // Multiply with ideal downchirp
            volk_32fc_x2_multiply_32fc(&dechirped[0], samples, &ref_downchirp[0], number_of_samples);

            for (int i = 0; i < number_of_samples; i++)
            {
                fft_in[i].r = dechirped[i].real();
                fft_in[i].i = dechirped[i].imag();
            }
            // do the FFT
            kiss_fft(cfg, fft_in, fft_out);

            // Get magnitude
            for (uint32_t i = 0u; i < number_of_samples; i++)
            {
                fft_mag_sq[i] = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
                tot_en += fft_mag_sq[i] / number_of_samples;
            }
            free(cfg);
            delete[] fft_in;
            delete[] fft_out;

            int max_idx = (std::max_element(fft_mag_sq, fft_mag_sq + number_of_samples) - fft_mag_sq);

            float sig_en = fft_mag_sq[max_idx] / number_of_samples;

            // use three bins instead of only one for an estimate less reliant on the synchronization
            float sig_and_noise_en_sum = 0;
            int n_bins = sig_and_noise_en_sum * 2 + 1;
            for (int i = -additional_symbol_bin; i <= additional_symbol_bin; i++)
            {
                sig_and_noise_en_sum += fft_mag_sq[lora_sdr::mod(max_idx + i, number_of_samples)] / number_of_samples;
            }

            float noise_pow = (tot_en - sig_and_noise_en_sum) / (number_of_samples - n_bins);

            float sig_pow = (sig_and_noise_en_sum - n_bins * noise_pow) / number_of_samples;

            // *out_noise_pow += (tot_en - sig_en) / (number_of_samples);
            // *out_signal_pow += sig_en / number_of_samples; // we want the signal power (energy per sample)
            // std::cout << "[lora_frame_sync_impl.cc] "<<m_identity<<" sig_pow "<<sig_pow<<", noise_pow "<<noise_pow<<", snr "<<10 * log10(sig_pow / noise_pow)<<std::endl;
            // return 10 * log10(sig_en / (tot_en - sig_en));

            *out_noise_pow = noise_pow;
            *out_signal_pow = sig_pow;
            return 10 * log10(sig_pow / noise_pow);
        }

        float lora_frame_sync_impl::estimate_CFO_frac(gr_complex *samples)
        {
            int k0[m_n_up];
            float cfo_frac;
            std::vector<gr_complex> CFO_frac_correc_aug(m_n_up * m_bins_per_symbol);
            double k0_mag[m_n_up];
            std::vector<gr_complex> fft_val(m_n_up * m_bins_per_symbol);

            std::vector<gr_complex> dechirped(m_bins_per_symbol);
            kiss_fft_cpx *fft_in = new kiss_fft_cpx[m_bins_per_symbol];
            kiss_fft_cpx *fft_out = new kiss_fft_cpx[m_bins_per_symbol];
            float fft_mag_sq[m_bins_per_symbol] = {0};

            kiss_fft_cfg cfg_cfo = kiss_fft_alloc(m_bins_per_symbol, 0, 0, 0);
            for (int ii = 0; ii < m_n_up; ii++)
            {
                // Dechirping
                volk_32fc_x2_multiply_32fc(&dechirped[0], &samples[m_bins_per_symbol * ii], &m_downchirp[0], m_bins_per_symbol);
                // prepare FFT
                for (int jj = 0; jj < m_bins_per_symbol; jj++)
                {
                    fft_in[jj].r = dechirped[jj].real();
                    fft_in[jj].i = dechirped[jj].imag();
                }
                // do the FFT
                kiss_fft(cfg_cfo, fft_in, fft_out);

                // Get magnitude
                for (int jj = 0; jj < m_bins_per_symbol; jj++)
                {
                    fft_mag_sq[jj] = fft_out[jj].r * fft_out[jj].r + fft_out[jj].i * fft_out[jj].i;
                    fft_val[jj + ii * m_bins_per_symbol] = gr_complex(fft_out[jj].r, fft_out[jj].i);
                }
                k0[ii] = std::max_element(fft_mag_sq, fft_mag_sq + m_bins_per_symbol) - fft_mag_sq;

                k0_mag[ii] = fft_mag_sq[k0[ii]];
            }

            free(cfg_cfo);
            delete[] fft_in;
            delete[] fft_out;
            // get argmax
            int idx_max = k0[std::max_element(k0_mag, k0_mag + m_n_up) - k0_mag];
            gr_complex four_cum(0.0f, 0.0f);
            for (int ii = 0; ii < m_n_up - 1; ii++)
            {
                four_cum += fft_val[idx_max + m_bins_per_symbol * ii] * std::conj(fft_val[idx_max + m_bins_per_symbol * (ii + 1)]);
            }
            cfo_frac = -std::arg(four_cum) / 2 / M_PI;
            // // Correct CFO in preamble
            // for (int n = 0; n < m_n_up * m_bins_per_symbol; n++)
            // {
            //     CFO_frac_correc_aug[n] = gr_expj(-2 * M_PI * cfo_frac / m_bins_per_symbol * n);
            // }
            // volk_32fc_x2_multiply_32fc(&preamble_upchirps[0], samples, &CFO_frac_correc_aug[0], m_n_up * m_bins_per_symbol);
            return cfo_frac;
        }

        float lora_frame_sync_impl::estimate_STO_frac(gr_complex *samples)
        {
            int k0;
            double Y_1, Y0, Y1, u, v, ka, wa, k_residual;
            float sto_frac = 0;

            std::vector<gr_complex> dechirped(m_bins_per_symbol);
            kiss_fft_cpx *fft_in = new kiss_fft_cpx[2 * m_bins_per_symbol];
            kiss_fft_cpx *fft_out = new kiss_fft_cpx[2 * m_bins_per_symbol];

            float fft_mag_sq[2 * m_bins_per_symbol] = {0};

            kiss_fft_cfg cfg_sto = kiss_fft_alloc(2 * m_bins_per_symbol, 0, 0, 0);

            for (int i = 0; i < m_n_up; i++)
            {
                // Dechirping
                volk_32fc_x2_multiply_32fc(&dechirped[0], &samples[m_bins_per_symbol * i], &m_downchirp[0], m_bins_per_symbol);

                // prepare FFT
                for (int j = 0; j < 2 * m_bins_per_symbol; j++)
                {
                    if (j < m_bins_per_symbol)
                    {
                        fft_in[j].r = dechirped[j].real();
                        fft_in[j].i = dechirped[j].imag();
                    }
                    else
                    { // add padding
                        fft_in[j].r = 0;
                        fft_in[j].i = 0;
                    }
                }
                // do the FFT
                kiss_fft(cfg_sto, fft_in, fft_out);
                // Get magnitude
                for (uint32_t j = 0u; j < 2 * m_bins_per_symbol; j++)
                {
                    fft_mag_sq[j] += fft_out[j].r * fft_out[j].r + fft_out[j].i * fft_out[j].i;
                }
            }
            free(cfg_sto);
            delete[] fft_in;
            delete[] fft_out;

            // get argmax here
            k0 = std::max_element(fft_mag_sq, fft_mag_sq + 2 * m_bins_per_symbol) - fft_mag_sq;

            // get three spectral lines
            Y_1 = fft_mag_sq[lora_sdr::mod(k0 - 1, 2 * m_bins_per_symbol)];
            Y0 = fft_mag_sq[k0];
            Y1 = fft_mag_sq[lora_sdr::mod(k0 + 1, 2 * m_bins_per_symbol)];

            // set constant coeff
            u = 64 * m_bins_per_symbol / 406.5506497; // from Cui yang (eq.15)
            v = u * 2.4674;
            // RCTSL
            wa = (Y1 - Y_1) / (u * (Y1 + Y_1) + v * Y0);
            ka = wa * m_bins_per_symbol / M_PI;
            k_residual = fmod((k0 + ka) / 2, 1);
            sto_frac = k_residual - (k_residual > 0.5 ? 1 : 0);

            return sto_frac;
        }

        int lora_frame_sync_impl::general_work(int noutput_items,
                                               gr_vector_int &ninput_items,
                                               gr_vector_const_void_star &input_items,
                                               gr_vector_void_star &output_items)
        {
            int work_id = 0;
            auto in = static_cast<const input_type *>(input_items[0]);
            auto out = static_cast<output_type *>(output_items[0]);

            int items_to_output = 0;
            int items_to_consume = 0;
            if (finished())
            {
                debug_print("work done frame_sync", m_identity, "lora_frame_sync", GREEN);
                return WORK_DONE;
            }

            // !extra care need to be taken for the first time the half of the input buffer get filled!
            int max_items_to_process = ninput_items[0] - m_buffer_read_offset;

            std::vector<tag_t> tags;
            get_tags_in_window(tags, 0, m_buffer_read_offset, ninput_items[0], pmt::string_to_symbol("work_info"));
            if (tags.size())
            {
                // process only samples until next tag (next frame begins)
                if (tags[0].offset > nitems_read(0) + m_buffer_read_offset)
                {
                    max_items_to_process = tags[0].offset - (nitems_read(0) + m_buffer_read_offset); // only use samples until the next frame begins (SF might change)
                    

                    debug_print("new frame tag: nitems_read "<<nitems_read(0)<<", tag offset " << tags[0].offset <<",m_buffer_read_offset "<<m_buffer_read_offset<<", max_items_to_process "<<max_items_to_process, "", "lora_frame_sync", GREEN);
                    
                    // if (max_items_to_process / m_samples_per_symbol < m_n_up) // if not enough samples until tag to try detecting the previous frame, just drop them and focus on the next
                    // {

                    // Flush buffer. only one frame can be in the buffer (after m_buffer_read_offset and end at any time)
                    items_to_consume += max_items_to_process;
                    goto return_work;
                    // }
                }
                else if (tags[0].offset < nitems_read(0) + m_buffer_read_offset) // tag in buffer margin (should be a previously found frame begin tag still in buffer)
                {
                    if (tags[0].offset != m_last_tag_offset)
                    {
                    
                        error_print("tag in the buffer margin! nitems_read " << nitems_read(0) << ", tag offset " << tags[0].offset,m_identity);
                    }
                    if (tags.size() >= 2)
                    {
                        std::cout << "[lora_frame_sync_impl.cc] " << m_identity << " Two tags in a single input buffer, this is suspiciously close \n";
                        for (int i = 0; i < tags.size(); i++)
                        {

                            error_print("tag in the buffer margin! nitems_read " << nitems_read(0) << ", tag offset " << tags[i].offset,m_identity);
                            
                        }
                        max_items_to_process = tags[1].offset - tags[0].offset; // two frames in the input buffer
                    }
                }
                else // we start a new frame
                {
                    pmt::pmt_t err = pmt::string_to_symbol("error");
                    if (tags.size() >= 2)
                    {
                        std::cout << "[lora_frame_sync_impl.cc] " << m_identity << " Two tags in a single input buffer, this is suspiciously close \n";
                        for (int i = 0; i < tags.size(); i++)
                        {
                           std::cout << RED << "[Error][lora_frame_sync_impl.cc] " << m_identity << " tag in the buffer margin! nitems_read " << nitems_read(0) << ", tag offset " << tags[i].offset << "\n"
                                  << RESET;

                        }
                        max_items_to_process = tags[1].offset - tags[0].offset; // two frames in the input buffer
                    }

                    m_last_tag_offset = tags[0].offset;
                    m_state = DETECT;
                    int sf = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("sf"), err));
                    set_sf(sf);
                    m_frame_tag = tags[0];
                    debug_print("new_frame Tag offset: " << tags[0].offset<<" sf "<<sf,"","lora_frame_sync",GREEN);


                    work_id = pmt::to_long(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("work_id"), err));

                    int64_t time_full = pmt::to_uint64(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("time_full"), err));
                    double time_frac = pmt::to_double(pmt::dict_ref(tags[0].value, pmt::string_to_symbol("time_frac"), err));

                    m_current_time = ::uhd::time_spec_t(time_full, time_frac);
                    debug_print("Tag relative offset: " << tags[0].offset - nitems_read(0) << ", time first sample of new work: " << m_current_time.get_full_secs() << "." << m_current_time.get_frac_secs(),"","lora_frame_sync",GREEN);


                    m_received_frame_tag = true;
                    // std::cout << "[lora_frame_sync_impl.cc] "<<m_identity<<" "<<work_id <<" new frame tag with correct offset " << tags[0].offset<<", nitemsread: "<<nitems_read(0) << " sf: " << sf << " work_id " << work_id <<" max_items_to_process " <<max_items_to_process<< "\n";

                    // check if enough input samples for new SF, if not, return from work and wait for a valid call
                    if (ninput_items[0] < m_buffer_read_offset + m_n_up * m_samples_per_symbol)
                    {
                        std::cout << "Not enough input samples to continue now " << ninput_items[0] << " instead of " << m_buffer_read_offset + m_n_up * m_samples_per_symbol << "\n";
                        return 0;
                    }
                    debug_print("[Time] new frame starts at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","lora_frame_sync",GREEN);
                    
                }
            }
            
            switch (m_state)
            {
            case DETECT:
            {
                // std::cout << "input items: " << ninput_items[0] << ", new_samples: " << new_samples << std::endl;
                for (int symb_id = 0; symb_id < max_items_to_process / m_samples_per_symbol - m_n_up + 1; symb_id++)
                {
                    // downsampling
                    for (int ii = 0; ii < m_bins_per_symbol * m_n_up; ii++) // TODO avoid doing this multiple time on the same samples
                    {
                        m_in_down[ii] = in[m_buffer_read_offset + symb_id * m_samples_per_symbol + m_os_factor * ii];
                    }

                    std::pair<int, float> max_bin_new = dechirp_and_fft(&m_in_down[0], &m_downchirp, m_n_up * m_bins_per_symbol);

                    // std::cout << "get max bin: " << max_bin_new.first << "," << max_bin_new.second << std::endl;
                    // check difference with previous max_bin (modulo and offset are there to account that values are cyclic. e.g. max_val is only 1 off from 0);
                    int diff_with_prev = abs(lora_sdr::mod(abs(max_bin_new.first - m_prev_bin_idx.first) + (m_n_up * m_bins_per_symbol / 2), m_n_up * m_bins_per_symbol) - m_n_up * m_bins_per_symbol / 2);

                    if ((diff_with_prev <= 2 /*|| m_found_repetitions*/) &&                     // look for consecutive upchirps values(with a margin of ±2)
                        (m_n_repetitions < (m_n_repetitions_req + m_max_additional_symbols)) && // and max number of repetition is not reached
                        max_bin_new.second > 0)                                                 // and ignore maximums with 0 amplitude which appears when simulation with no noise
                    {
                        if (m_n_repetitions == 1)
                        {
                            // m_preamb_up_vals[0] = m_prev_bin_idx;
                            m_preamb_up_vals.push_back(m_prev_bin_idx);
                        }
                        // m_preamb_up_vals[m_n_repetitions] = max_bin_new;
                        m_preamb_up_vals.push_back(max_bin_new);

                        // std::cout<<"[lora_frame_sync_impl.cc] symb_start idx: "<<nitems_read(0)+m_buffer_read_offset + symb_id * m_samples_per_symbol<<", max_bin_new: "<<max_bin_new.first<<" "<<max_bin_new.second<<std::endl;

                        // memcpy(&preamble_raw[m_bins_per_symbol * symbol_cnt], &in_down[0], m_bins_per_symbol * sizeof(gr_complex));
                        // memcpy(&preamble_raw_up[m_samples_per_symbol * symbol_cnt], &in[(int)(m_os_factor / 2)], m_samples_per_symbol * sizeof(gr_complex));

                        m_n_repetitions++;
                    }
                    else // non repeating values or max number of repetitions
                    {
                        if (m_found_repetitions) // we must proceed with the synchronization
                        {
                            m_state = SYNC;
                            debug_print("[Time] preamble_repetition found at: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","lora_frame_sync",GREEN);

                            // std::cout << "Detection occured at: " << nitems_read(0) << " with " << (int)m_n_repetitions << " found" << std::endl;
                            // for (size_t i = 0; i < m_preamb_up_vals.size(); i++)
                            // {
                            //     std::cout << m_preamb_up_vals[i].first << " " << m_preamb_up_vals[i].second << ", ";
                            // }
                            // std::cout << std::endl;
                            // find the most frequent symbol value
                            int indices[m_preamb_up_vals.size()] = {};
                            for (int ii = 0; ii < m_preamb_up_vals.size(); ii++)
                            {
                                indices[ii] = m_preamb_up_vals[ii].first;
                            }
                            int k_hat = lora_sdr::most_frequent(&indices[0], m_preamb_up_vals.size());

                            auto max_it = max_element(m_preamb_up_vals.begin(), m_preamb_up_vals.end(), [](const auto &lhs, const auto &rhs)
                                                      { return lhs.second < rhs.second; });
                            int max_idx = max_it - m_preamb_up_vals.begin();

                            // std::cout << "k_hat " << k_hat << ", max_idx: " << max_idx << std::endl;

                            // clear the buffer until the first candidate offset or the begining of the available symbols
                            items_to_consume += std::max(0, (max_idx - (m_n_sync_candidates - 1) / 2) * (int)m_samples_per_symbol);
                            // std::cout << "consume extra " << std::max(0,(max_idx - (m_n_sync_candidates - 1) / 2) * (int)m_samples_per_symbol) << ", samples" << std::endl;

                            // add rough synchronization (demodulated symbols should be 0)
                            items_to_consume += m_os_factor * ((int)(m_bins_per_symbol - k_hat));
                            items_to_consume -= m_samples_per_symbol;

                            goto return_work; // return to have clean buffer for synchro
                        }
                        else
                        { // reset counter
                            m_n_repetitions = 1;
                            m_preamb_up_vals.clear();
                        }
                    }
                    m_prev_bin_idx = max_bin_new;
                    if (m_n_repetitions >= (int)(m_n_repetitions_req)) // if enough repetitions are found
                    {
                        m_found_repetitions = true;

                        // if ((int)(m_n_repetitions - m_n_repetitions_req) >= m_additional_symbols_req) // we should start the synchronization steps
                        // {
                        // m_state = SYNC;
                        // std::cout << "Detection occured at: " << nitems_read(0) << std::endl;
                        // symbol_cnt = 0;
                        // m_n_repetitions = 1;
                        // for (size_t i = 0; i < m_preamb_up_vals.size(); i++)
                        // {
                        //     std::cout << m_preamb_up_vals[i].first << " " << m_preamb_up_vals[i].second << ", ";
                        // }
                        // std::cout << std::endl;

                        // // find the most frequent symbol value
                        // int indices[m_preamb_up_vals.size()] = {};
                        // for (int ii = 0; ii < m_preamb_up_vals.size(); ii++)
                        // {
                        //     indices[ii] = m_preamb_up_vals[ii].first;
                        // }
                        // int k_hat = most_frequent(&indices[0], m_preamb_up_vals.size());

                        // auto max_it = max_element(m_preamb_up_vals.begin(), m_preamb_up_vals.end(), [](const auto &lhs, const auto &rhs)
                        //                           { return lhs.second < rhs.second; });
                        // int max_idx = max_it - m_preamb_up_vals.begin();

                        // // std::cout << "k_hat " << k_hat << ", max_idx: " << max_idx << std::endl;

                        // // clear the buffer until the first candidate offset
                        // items_to_consume += (max_idx - (m_n_sync_candidates - 1) / 2) * m_samples_per_symbol;
                        // // std::cout << "consume extra " << (max_idx - (m_sync_candidates - 1) / 2) * m_samples_per_symbol << ", samples" << std::endl;

                        // // add rough synchronization (demodulated symbols should be 0)
                        // items_to_consume += m_os_factor * ((int)(m_bins_per_symbol - k_hat));
                        // goto return_work; // return to have clean buffer for synchro
                        // }
                    }
                    items_to_consume += m_samples_per_symbol;
                }
                break;
            }
            case SYNC:
            {
                m_preamb_up_vals.clear();
                m_found_repetitions = false;
                int symb_rep_offset = m_n_repetitions_req + m_max_additional_symbols - m_n_repetitions; // offset based on number of repetitions found
                m_synchro_read_offset = m_buffer_read_offset - m_n_repetitions * m_samples_per_symbol;
                
                // std::cout << "Resync m_n_repetitions_req: " << (int)m_n_repetitions_req << ", m_max_additional_symbols " << (int)m_max_additional_symbols << ", and m_n_repetitions: " << (int)m_n_repetitions << ", m_buffer_read_offset " << m_buffer_read_offset << std::endl;
                // std::cout << "Resync offset: " << (int)(symb_rep_offset)*m_samples_per_symbol + m_samples_per_symbol + m_os_factor * (1) << ", and new one: " << (int)m_synchro_read_offset << std::endl;
                m_n_repetitions = 1;
                for (int candidate_id = 0; candidate_id < m_sync_candidates.size(); candidate_id++)
                {
                    sync_info new_sync_candidate;
                    // decimate input buffer
                    std::vector<gr_complex> preamb_upchirps_down(m_bins_per_symbol * m_n_up);

                    for (int ii = 0; ii < preamb_upchirps_down.size(); ii++)
                    {
                        preamb_upchirps_down[ii] = in[m_synchro_read_offset + candidate_id * m_samples_per_symbol + m_os_factor * ii]; //+m_samples_per_symbol to be able to resync based on integer STO and CFO estimate, and +1 for fractional sto
                    }

                    new_sync_candidate.cfo_frac = estimate_CFO_frac(preamb_upchirps_down.data());

                    // Correct fractional CFO in preamble
                    std::vector<gr_complex> preamble_cfoint_sto_sfo(m_bins_per_symbol * (m_n_up + 2 + 2.25 + 2)); // +2 for the 2 network ids, +2.25 for the downchirps, and +2 for one symbol margin at each end, for STO/CFO compensation
                    for (int ii = 0; ii < preamble_cfoint_sto_sfo.size(); ii++)
                    {
                        preamble_cfoint_sto_sfo[ii] = in[m_synchro_read_offset + (candidate_id - 1) * m_samples_per_symbol + m_os_factor * ii] * gr_expj(-2 * M_PI * new_sync_candidate.cfo_frac / m_bins_per_symbol * ii);
                    }

                    // demodulate downchirps
                    std::pair<int, float> down_val = dechirp_and_fft(&preamble_cfoint_sto_sfo[m_bins_per_symbol * (m_n_up + 2 + 1)], &m_upchirp, 2 * m_bins_per_symbol); //+2 for the 2 network ids, +1 for the front margin for CFO/STO int compensation

                    std::pair<int, float> up_val = dechirp_and_fft(&preamble_cfoint_sto_sfo[m_bins_per_symbol], &m_downchirp, m_n_up * m_bins_per_symbol);

                    // std::cout << "up val: " << up_val.first<<","<<up_val.second << ", down val " << down_val.first <<","<<down_val.second<< std::endl;
                    // if (up_val.first != 0)
                    // {
                    //     std::cout << RED <<"candidate "<< candidate_id<<": up val not zero, but " << up_val.first << " Down val: " << down_val.first << RESET << std::endl;
                    // }
                    // if ((up_val.first + down_val.first) % 2 > 0)
                    // {
                    //     std::cout << RED <<"candidate "<< candidate_id<< ": Ambiguous values for CFOint and STOint: up val: " << up_val.first << " Down val: " << down_val.first << RESET << std::endl;
                    // }

                    // Use downchirp value to distinguish integer STO and integer CFO
                    new_sync_candidate.cfo_int = floor(((up_val.first + down_val.first) / 2) % (m_bins_per_symbol / 2));
                    new_sync_candidate.cfo_int -= (new_sync_candidate.cfo_int < (m_bins_per_symbol / 4) ? 0 : (m_bins_per_symbol / 2));

                    // std::cout << "cfo int est = " << new_sync_candidate.cfo_int << std::endl;

                    new_sync_candidate.sto_int = (up_val.first - new_sync_candidate.cfo_int) % m_bins_per_symbol;
                    // remap sto int [0;N[ -> [-N/2,N/2[
                    new_sync_candidate.sto_int -= (new_sync_candidate.sto_int < (m_bins_per_symbol / 2) ? 0 : (m_bins_per_symbol));

                    // Estimate SFO from CFO (this assumes the same oscillator generate both frequencies)
                    new_sync_candidate.sfo = float((new_sync_candidate.cfo_int + new_sync_candidate.cfo_frac) * m_bw) / m_center_freq / m_bins_per_symbol;
                    double fs = m_bw;
                    double fs_p = m_bw * (1 - new_sync_candidate.sfo);
                    int N = m_bins_per_symbol;

                    // std::cout << "clk offset est = " << new_sync_candidate.sfo << std::endl;

                    // compensate cfo int, sto int and sfo in the received preamble
                    std::vector<gr_complex> preamble_tmp(int((m_n_up + 2 + 2.25) * m_bins_per_symbol));

                    for (int ii = 0; ii < preamble_tmp.size(); ii++)
                    {
                        // Shift based on STO int and compensate CFO int
                        gr_complex cfo_int_comp = gr_expj(-2 * M_PI * new_sync_candidate.cfo_int / m_bins_per_symbol * ii);
                        gr_complex sfo_comp = gr_expj(-2 * M_PI * (pow(m_bw, 2) * pow(lora_sdr::mod(ii, N), 2) / (2 * N) * (pow(fs, 2) - pow(fs_p, 2)) / (pow(fs, 2) * pow(fs_p, 2)) + (std::floor((float)ii / N) * (pow(m_bw / fs_p, 2) - m_bw / fs_p) + m_bw / 2 * ((fs_p - fs) / (fs * fs_p))) * lora_sdr::mod(ii, N)));
                        preamble_tmp[ii] = preamble_cfoint_sto_sfo[m_bins_per_symbol - new_sync_candidate.sto_int + ii] * cfo_int_comp * sfo_comp;
                    }

                    // estimate frac STO after integer offset correction to get synchronized preamble
                    new_sync_candidate.sto_frac = estimate_STO_frac(preamble_tmp.data());

                    debug_print("offset estimate: STO_int: " << new_sync_candidate.sto_int << ", STO_frac: " << new_sync_candidate.sto_frac << ", CFO_int: " << new_sync_candidate.cfo_int << ", CFO_frac: " << new_sync_candidate.cfo_frac << ", SFO: " << new_sync_candidate.sfo,"","lora_frame_sync",GREEN);
                    std::vector<gr_complex> preamble(int((m_n_up + 2 + 2.25) * m_bins_per_symbol));

                    for (int ii = 0; ii < preamble.size(); ii++)
                    {
                        gr_complex cfo_comp = gr_expj(-2 * M_PI * (new_sync_candidate.cfo_int + new_sync_candidate.cfo_frac) / m_bins_per_symbol * ii);
                        gr_complex sfo_comp = gr_expj(-2 * M_PI * (pow(m_bw, 2) * pow(lora_sdr::mod(ii, N), 2) / (2 * N) * (pow(fs, 2) - pow(fs_p, 2)) / (pow(fs, 2) * pow(fs_p, 2)) + (std::floor((float)ii / N) * (pow(m_bw / fs_p, 2) - m_bw / fs_p) + m_bw / 2 * ((fs_p - fs) / (fs * fs_p))) * lora_sdr::mod(ii, N)));
                        // compensate sto
                        preamble[ii] = in[m_synchro_read_offset + (candidate_id - 1) * m_samples_per_symbol + (m_bins_per_symbol - new_sync_candidate.sto_int) * m_os_factor + std::lround(m_os_factor * (ii - new_sync_candidate.sto_frac))];
                        // compensate cfo and sfo
                        preamble[ii] *= cfo_comp * sfo_comp;
                    }

                    // Get downchirp and upchirp values after offsets corrections
                    down_val = dechirp_and_fft(&preamble[m_bins_per_symbol * (m_n_up + 2)], &m_upchirp, 2 * m_bins_per_symbol); //+2 for the 2 network ids, +1 for the front margin for CFO/STO int compensation
                    up_val = dechirp_and_fft(&preamble[0], &m_downchirp, m_n_up * m_bins_per_symbol);

                    if (down_val.first != 0 || up_val.first != 0)
                    {
                        // Use downchirp value to distinguish integer STO and integer CFO
                        int cfo_int_diff = floor(((up_val.first + down_val.first) / 2) % (m_bins_per_symbol / 2));
                        cfo_int_diff -= (cfo_int_diff < (m_bins_per_symbol / 4) ? 0 : (m_bins_per_symbol / 2));

                        int sto_int_diff = (up_val.first - cfo_int_diff) % m_bins_per_symbol;
                        // remap sto int [0;N[ -> [-N/2,N/2[
                        sto_int_diff -= (sto_int_diff < (m_bins_per_symbol / 2) ? 0 : (m_bins_per_symbol));
                        new_sync_candidate.sto_int += sto_int_diff;
                        new_sync_candidate.cfo_int += cfo_int_diff;
                        // resynch with new offsets
                        for (int ii = 0; ii < preamble.size(); ii++)
                        {
                            gr_complex cfo_comp = gr_expj(-2 * M_PI * (new_sync_candidate.cfo_int + new_sync_candidate.cfo_frac) / m_bins_per_symbol * ii);
                            gr_complex sfo_comp = gr_expj(-2 * M_PI * (pow(m_bw, 2) * pow(lora_sdr::mod(ii, N), 2) / (2 * N) * (pow(fs, 2) - pow(fs_p, 2)) / (pow(fs, 2) * pow(fs_p, 2)) + (std::floor((float)ii / N) * (pow(m_bw / fs_p, 2) - m_bw / fs_p) + m_bw / 2 * ((fs_p - fs) / (fs * fs_p))) * lora_sdr::mod(ii, N)));
                            // compensate sto
                            preamble[ii] = in[m_synchro_read_offset + (candidate_id - 1) * m_samples_per_symbol + (m_bins_per_symbol - new_sync_candidate.sto_int) * m_os_factor + std::lround(m_os_factor * (ii - new_sync_candidate.sto_frac))];
                            // compensate cfo and sfo
                            preamble[ii] *= cfo_comp * sfo_comp;
                        }
                    }

                    // Estimate noise and signal power, symbol by symbol
                    new_sync_candidate.noise_power = 0;
                    new_sync_candidate.signal_power = 0;

                    float noise_power_tmp;
                    float sig_power_tmp;

                    float snr_est_tmp = 0;
                    for (int ii = 0; ii < m_n_up + 2; ii++) //+2 for the two network identifiers
                    {
                        snr_est_tmp += determine_snr(&preamble[ii * m_bins_per_symbol], m_downchirp.data(), &noise_power_tmp, &sig_power_tmp, m_bins_per_symbol, 1);
                        new_sync_candidate.noise_power += noise_power_tmp;
                        new_sync_candidate.signal_power += sig_power_tmp;
                    }
                    snr_est_tmp /= m_n_up + 2;
                    new_sync_candidate.noise_power /= m_n_up + 2;
                    new_sync_candidate.signal_power /= m_n_up + 2;

                    // Estimate noise and signal power of the full preamble
                    std::vector<gr_complex> m_ref_preamble_conj(size(m_ref_preamble));
                    volk_32fc_conjugate_32fc(m_ref_preamble_conj.data(), m_ref_preamble.data(), m_ref_preamble.size());

                    new_sync_candidate.upchirps_snr_est = determine_snr(&preamble[0], m_ref_preamble_conj.data(), &noise_power_tmp, &sig_power_tmp, m_n_up * m_bins_per_symbol, m_n_up); // Estimate the SNR using the all upchirps

                    debug_print(" Full preamble snr estimate: " << snr_est_tmp, m_identity,"lora_frame_sync",GREEN);
                    debug_print(work_id << " sig pow indiv: "<<10*log10(new_sync_candidate.signal_power)<<" over all preamble: "<< 10*log10(sig_power_tmp),m_identity,"lora_frame_sync",GREEN);
                    debug_print(work_id << " noise pow indiv: "<<10*log10(new_sync_candidate.noise_power)<<" over all preamble: "<< 10*log10(noise_power_tmp),m_identity,"lora_frame_sync",GREEN);

//---- FOR DEBUGGING ---
// #ifdef LORA_SYNC_DEBUG
//                     new_sync_candidate.corrected_preamb.resize(preamble.size());
//                     memcpy(new_sync_candidate.corrected_preamb.data(), preamble.data(), preamble.size() * sizeof(gr_complex));
// #endif
                    //--------------------

                    // //correlate with reference preamble
                    // gr_complex corr_res;
                    // volk_32fc_x2_conjugate_dot_prod_32fc(&corr_res, preamble.data(), m_ref_preamble.data(), preamble.size());
                    // new_sync_candidate.corr_with_ref_pre = std::abs(corr_res);
                    // // get energy in correlator
                    // std::vector<float> corr_en(preamble.size());
                    // volk_32fc_magnitude_squared_32f(corr_en.data(), preamble.data(), preamble.size());
                    // volk_32f_accumulator_s32f(&new_sync_candidate.corr_en, corr_en.data(), corr_en.size());

                    // std::cout << "sig pow: " << signal_pow << ", noise var: " << noise_pow << ", SNR: " << snr_est << std::endl;

                    m_sync_candidates[candidate_id] = new_sync_candidate;
                }

                int max_idx = std::max_element(m_sync_candidates.begin(), m_sync_candidates.end(), [](const sync_info &a, const sync_info &b)
                                               { return a.upchirps_snr_est < b.upchirps_snr_est; }) -
                              m_sync_candidates.begin();

                std::cout <<m_identity<< "correlation result:------------------------------- \n";
                for (int i = 0; i < m_sync_candidates.size(); i++)
                {
                    if (max_idx == i)
                        std::cout << GREEN << "candidate " << i << ": snr upchirps = " << m_sync_candidates[i].upchirps_snr_est << ", cfo = " << m_sync_candidates[i].cfo_frac + m_sync_candidates[i].cfo_int << ", sto = " << m_sync_candidates[i].sto_frac + m_sync_candidates[i].sto_int << ", sfo = " << m_sync_candidates[i].sfo << ", snr by symbol = " << 10 * log10(m_sync_candidates[i].signal_power / m_sync_candidates[i].noise_power) << "\n"
                                  << RESET;
                    else
                        std::cout << "candidate " << i << ": corr = " << m_sync_candidates[i].upchirps_snr_est << ", cfo = " << m_sync_candidates[i].cfo_frac + m_sync_candidates[i].cfo_int << ", sto = " << m_sync_candidates[i].sto_frac + m_sync_candidates[i].sto_int << ", sfo = " << m_sync_candidates[i].sfo << ", snr by symbol = " << 10 * log10(m_sync_candidates[i].signal_power / m_sync_candidates[i].noise_power) << "\n";
                }

                // check if the SNR estimated after synchro is sufficient to consider it
                // float corr = m_sync_candidates[max_idx].corr_with_ref_pre;
                // float preamb_en = m_sync_candidates[max_idx].corr_en;
                // float ref_preamb_en = m_ref_preamble.size();

                float snr_est = m_sync_candidates[max_idx].upchirps_snr_est;
                float symb_by_symb_snr_est = 10 * log10(m_sync_candidates[max_idx].signal_power / m_sync_candidates[max_idx].noise_power);

                debug_print("Work " << work_id << " frame found with: SNR est symb-by-symb " << symb_by_symb_snr_est << " and all upchirps snr est " << snr_est << ", sto frac :" << m_sync_candidates[max_idx].sto_frac,"","lora_frame_sync",GREEN);
                debug_print("Final offset estimate: STO_int: " << m_sync_candidates[max_idx].sto_int << ", STO_frac: " << m_sync_candidates[max_idx].sto_frac << ", CFO_int: " << m_sync_candidates[max_idx].cfo_int << ", CFO_frac: " << m_sync_candidates[max_idx].cfo_frac << ", SFO: " << m_sync_candidates[max_idx].sfo,"","lora_frame_sync",GREEN);

                if (abs(snr_est - symb_by_symb_snr_est) > m_max_snr_diff)
                {
                    debug_print(" SNR estimate difference too high: " << snr_est << " vs " << symb_by_symb_snr_est,"","lora_frame_sync",RED);
                    // m_received_frame_tag = false;
                    m_state = DETECT;
                    goto return_work;
                }

                m_sfo_symb = m_sync_candidates[max_idx].sfo * m_bins_per_symbol;
                // align buffer to preamble start

                items_to_consume = max_idx * m_samples_per_symbol + std::lround(m_os_factor * (-m_sync_candidates[max_idx].sto_frac)) - m_sync_candidates[max_idx].sto_int * m_os_factor;

                // align to payload start
                items_to_consume += m_samples_per_symbol * (m_n_up + 2 + 2.25);

                debug_print("[Time] m_synchro_read_offset "<<m_synchro_read_offset<<", m_buffer_read_offset "<<m_buffer_read_offset<<", " ,"","lora_frame_sync",GREEN);

                pmt::pmt_t preamb_info = pmt::make_dict();
                preamb_info = pmt::dict_add(preamb_info, pmt::intern("new_preamb"), pmt::from_bool(true));
                message_port_pub(pmt::intern("preamb_info"), preamb_info);

                // std::cout << "Preamble found after " << nitems_read(0) / m_samples_per_symbol << " processed symbols." << std::endl;
                // update sto_frac to its value at the payload beginning
                // m_sto_frac += sfo_hat * 4.25;
                // sfo_cum = ((m_sto_frac * m_os_factor) - my_roundf(m_sto_frac * m_os_factor)) / m_os_factor;
                // add new info to tag
                if (!m_received_frame_tag) // if we didn't received some information via the tags initialise a new one
                {
                    m_frame_tag.value = pmt::make_dict();
                    m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("sf"), pmt::mp((long)m_sf));
                    error_print(" we didn't received a tag!!\n", m_identity);
                }

                // std::cout << "tag received? "<<m_received_frame_tag << "\n";
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("is_header"), pmt::from_bool(true));
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("bandwidth"), pmt::from_uint64(m_bw));
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("cfo_int"), pmt::mp((long)m_sync_candidates[max_idx].cfo_int));
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("cfo_frac"), pmt::mp((float)m_sync_candidates[max_idx].cfo_frac));
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("noise_pow"), pmt::mp(m_sync_candidates[max_idx].noise_power));
                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("sig_pow"), pmt::mp(m_sync_candidates[max_idx].signal_power));

                m_frame_tag.key = pmt::intern("frame_info");
                m_frame_tag.offset = nitems_written(0);
                // add_item_tag(0, m_frame_tag);
                m_received_frame_tag = false;

                m_is_first_sample = true;

                m_received_head = true; 

                // items_to_consume += m_samples_per_symbol / 4 + m_os_factor * m_cfo_int;
                symbol_cnt = 0;
                m_sfo_cum = 0;

                debug_print("[Time] Synchro done at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","lora_frame_sync",GREEN);
                
                m_state = SFO_COMPENSATION;
                break;
            }
            case SFO_COMPENSATION:
            {
                // get number of symbols we can process
                int symbols_to_process = std::floor(std::min(max_items_to_process / m_samples_per_symbol, noutput_items / m_bins_per_symbol));

                if (m_is_first_sample)
                {
                    m_is_first_sample = false;
                    debug_print("[Time] start SFO comp at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us","","lora_frame_sync",GREEN);


                    m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("time_full"), pmt::mp(m_current_time.get_full_secs()));
                    m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::intern("time_frac"), pmt::mp(m_current_time.get_frac_secs()));

                    debug_print("[lora_frame_sync_impl] new frame sync time: " << std::setprecision(6) << std::fixed << (m_current_time.get_full_secs() + m_current_time.get_frac_secs()) << std::defaultfloat << " s","","lora_frame_sync",GREEN);

                    add_item_tag(0, m_frame_tag);
                }

                for (int symb_id = 0; symb_id < symbols_to_process; symb_id++)
                {

                    // transmit only useful symbols (at least 8 symbol for PHY header) or everything if  m_symb_numb set to 0
                    if (symbol_cnt < 8 || (symbol_cnt < m_symb_numb && m_received_head) || m_symb_numb == 0)
                    {
                        // output downsampled signal (with no STO but with CFO!)
                        for (int ii = 0; ii < m_bins_per_symbol; ii++)
                        {
                            out[ii + m_bins_per_symbol * symb_id] = in[m_synchro_read_offset + ii * m_os_factor + m_samples_per_symbol * symb_id];
                        }
                        items_to_output += m_bins_per_symbol;
                        items_to_consume += m_samples_per_symbol;

                        // update sfo evolution
                        if (abs(m_sfo_cum) > 1.0 / 2 / m_os_factor)
                        {
                            items_to_consume -= (-2 * signbit(m_sfo_cum) + 1);
                            m_sfo_cum -= (-2 * signbit(m_sfo_cum) + 1) * 1.0 / m_os_factor;
                        }
                        m_sfo_cum += m_sfo_symb;
                        symbol_cnt++;
                    }
                    else if (!m_received_head)
                    { // Wait for the header to be decoded
                    }
                    else
                    {
                        //never go in this state, reset managed externally
                        m_state = DETECT;
                        goto return_work;
                    }
                }
                break;
            }
            default:
            {
                error_print("Should not fall into default state!","");
                items_to_consume += ninput_items[0];
                break;
            }
            }
        return_work:
            // Tell runtime system how many input items we consumed on
            // each input stream.

            consume_each(items_to_consume);

            // update time
            m_current_time += (double)items_to_consume / m_bw / m_os_factor;

            // Tell runtime system how many output items we produced.
            debug_print("frame sync output "<<items_to_output<<" items in case "<<(int)m_state,"","lora_frame_sync",RED);
            return items_to_output;
        }

    } /* namespace cran */
} /* namespace gr */
