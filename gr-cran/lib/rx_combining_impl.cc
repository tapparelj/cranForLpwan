/* -*- c++ -*- */
/*
 * Copyright 2023 Tapparel Joachim @EPFL, TCL.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rx_combining_impl.h"
#include <gnuradio/io_signature.h>

namespace gr
{
    namespace cran
    {
        using output_type = double;
        rx_combining::sptr rx_combining::make(std::string input_addr)
        {
            return gnuradio::make_block_sptr<rx_combining_impl>(input_addr);
        }

        /*
         * The private constructor
         */
        rx_combining_impl::rx_combining_impl(std::string input_addr)
            : gr::block("rx_combining",
                        gr::io_signature::make(0, 0, 0),
                        gr::io_signature::make(1 /* min outputs */, 1 /*max outputs */,
                                               SF_MAX * sizeof(output_type)))
        {
            sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));

            // zmq binding
            m_input_socket.bind(input_addr);
            m_input_socket.set(zmq::sockopt::linger, 0);
            m_pollitems[0] = {m_input_socket, 0, ZMQ_POLLIN, 0};
            debug_print(" input socket bind to " << input_addr, m_identity, "rx_combining", BLUE);

            // register message ports
            message_port_register_in(pmt::mp("frame_info"));
            set_msg_handler(pmt::mp("frame_info"), [this](pmt::pmt_t msg)
                            { this->frame_info_handler(msg); });
            m_work_done = false;
            m_output_symbol_count = 0;
            numb_processed_symb = 0;
            m_symb_numb = 0;
            m_sf = 0; // undefined until tag
            m_force_long_wait = false;
        }
        struct membuf : std::streambuf
        {
            membuf(void *b, size_t len)
            {
                char *bc = static_cast<char *>(b);
                this->setg(bc, bc, bc + len);
            }
        };

        void rx_combining_impl::free_worker_for_next_work(std::string msg)
        {
            debug_print(msg << ", demod worker free for next work", m_identity, "rx_combining", CYAN);
            int in_buff_msg_cnt = 0;
            while (1) // clear input buffer
            {
                zmq::poll(m_pollitems, 1, std::chrono::milliseconds(0));
                if ((m_pollitems[0].revents & ZMQ_POLLIN)) // got smth from a synchronizer
                {
                    m_sync_worker_addr = s_recv(m_input_socket);
                    zmq::recv_result_t msg_size = m_input_socket.recv(m_msg_buff);
                    in_buff_msg_cnt++;
                }
                else
                {
                    break;
                }
            }
            debug_print(" had to clear " << in_buff_msg_cnt << " messages in IPC buffer", m_identity, "rx_combining", BLUE);
            m_rrh_params.clear();
            m_output_symbol_count = 0;
            numb_processed_symb = 0;
            m_symb_numb = 0;
            m_first_rrh_time_received = false;
            m_work_done = false;
        }
        void rx_combining_impl::frame_info_handler(pmt::pmt_t frame_info)
        {
            pmt::pmt_t err = pmt::string_to_symbol("error");
            bool m_impl_head = false; // explicit header enforced
            int m_cr = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("cr"), err));
            int m_pay_len = pmt::to_double(pmt::dict_ref(frame_info, pmt::string_to_symbol("pay_len"), err));
            int m_has_crc = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("crc"), err));
            uint8_t ldro_mode = pmt::to_long(pmt::dict_ref(frame_info, pmt::string_to_symbol("ldro_mode"), err));
            int m_invalid_header = pmt::to_double(pmt::dict_ref(frame_info, pmt::string_to_symbol("err"), err));

            if (m_invalid_header)
            {
                m_work_done = true;
                // free_worker_for_next_work("Incorrect header checksum");
            }
            else
            {
                if (ldro_mode == 2) // 2 corresponds to auto c.f utilities.h of gr-lora_sdr
                {
                    m_ldro = (float)(1u << m_sf) * 1e3 / m_bw > LDRO_AUTO_MAX_DURATION_MS;
                }
                else
                {
                    m_ldro = ldro_mode;
                }

                debug_print("Received header decoded should continue processing: cr=" << m_cr << ", pay_len=" << m_pay_len << ", CRC=" << m_has_crc, m_identity, "rx_combining", BLUE);
                m_symb_numb = 8 + ceil((double)(2 * m_pay_len - m_sf + 2 + !m_impl_head * 5 + m_has_crc * 4) / (m_sf - 2 * m_ldro)) * (4 + m_cr);
                frame_info = pmt::dict_add(frame_info, pmt::intern("is_header"), pmt::from_bool(false));
                frame_info = pmt::dict_add(frame_info, pmt::intern("symb_numb"), pmt::from_long(m_symb_numb));
                frame_info = pmt::dict_delete(frame_info, pmt::intern("ldro_mode"));

                frame_info = pmt::dict_add(frame_info, pmt::intern("ldro"), pmt::from_bool(m_ldro));
                // add header info tag to the output
                add_item_tag(0, nitems_written(0), pmt::string_to_symbol("frame_info"), frame_info);
                m_decoded_header = true;
                m_force_long_wait = true;
            }
        }

        bool rx_combining_impl::compute_rice(const float *bins, float *output, int size, float signal_power, float noise_pow)
        {
            float pow_scaling = 1.0 / noise_pow; // TODO might define scaling to prevent overflow as much as possible
            noise_pow = 1.0;
            float x;
            float v = std::sqrt(signal_power * pow_scaling * size);
            bool overflow = false;
            for (int i = 0; i < size; i++)
            {
                x = bins[i] * sqrt(pow_scaling);
                if (x * v / noise_pow > 500)
                {
                    overflow = true;
                }
                else if ((x * x + v * v) / (2 * noise_pow) > 500)
                {
                    overflow = true;
                }
                else
                {
                    output[i] = x / noise_pow * exp((long double)-(x * x + v * v) / (2 * noise_pow)) * boost::math::cyl_bessel_i((long double)0, (long double)x * v / noise_pow); // std::cyl_bessel_il(0,x*v/noise_pow);
                }
            }
            if (overflow) // just set the argmax to one, others to 1e-9
            {
                // error_print("Rician function is overflowing, numerical value > 1e-308!", m_identity);
                return false;
            }
            else
            {
                return true;
            }
        }
        bool rx_combining_impl::compute_scaled_rayleigh(const float *bins, float *output, int size, float noise_pow, float scaling_factor)
        {
            float x;
            bool overflow = false;
            for (int i = 0; i < size; i++)
            {
                x = bins[i];
                output[i] = x / noise_pow * exp((long double)-(x * x) / (2 * noise_pow)) * scaling_factor;
                if ((x * x) / (2 * noise_pow) > 500)
                {
                    overflow = true;
                }
            }
            if (overflow)
            {
                // error_print("Rayleigh computation is overflowing, numerical value > 1e-308!", m_identity);
                return false;
            }
            else
            {
                return true;
            }
        }

        void rx_combining_impl::compute_dechirp_fft_mag(const gr_complex *samples, gr_complex *ref_downchirp, float *out_fft_mag)
        {
            kiss_fft_cfg cfg = kiss_fft_alloc(m_samples_per_symbol, 0, 0, 0);
            kiss_fft_cpx *cx_in = new kiss_fft_cpx[m_samples_per_symbol];
            kiss_fft_cpx *cx_out = new kiss_fft_cpx[m_samples_per_symbol];

            // Dechirping: Multiply with ideal downchirp
            volk_32fc_x2_multiply_32fc(&m_dechirped[0], samples, &ref_downchirp[0], m_samples_per_symbol);

            for (int i = 0; i < m_samples_per_symbol; i++)
            {
                cx_in[i].r = m_dechirped[i].real();
                cx_in[i].i = m_dechirped[i].imag();
            }
            // do the FFT
            kiss_fft(cfg, cx_in, cx_out);

            // Get magnitude squared

            for (uint32_t i = 0u; i < m_samples_per_symbol; i++)
            {
                out_fft_mag[i] = std::sqrt((cx_out[i].r * cx_out[i].r + cx_out[i].i * cx_out[i].i) / m_samples_per_symbol);
            }

            free(cfg);
            delete[] cx_in;
            delete[] cx_out;
        }

        size_t parse_tag_header(zmq::message_t &msg,
                                uint64_t &offset_out,
                                std::vector<gr::tag_t> &tags_out)
        {
            membuf sb(msg.data(), msg.size());
            std::istream iss(&sb);

            size_t min_len =
                sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint64_t);
            if (msg.size() < min_len)
                error_print("incoming zmq msg too small to hold gr tag header!", "");

            uint16_t header_magic;
            uint8_t header_version;
            uint64_t rcv_ntags;

            iss.read((char *)&header_magic, sizeof(uint16_t));
            iss.read((char *)&header_version, sizeof(uint8_t));

            if (header_magic != GR_HEADER_MAGIC)
                error_print("gr header magic does not match!", "");

            if (header_version != 1)
                error_print("gr header version too high!", "");

            iss.read((char *)&offset_out, sizeof(uint64_t));
            iss.read((char *)&rcv_ntags, sizeof(uint64_t));

            for (size_t i = 0; i < rcv_ntags; i++)
            {
                gr::tag_t newtag;
                sb.sgetn((char *)&(newtag.offset), sizeof(uint64_t));
                newtag.key = pmt::deserialize(sb);
                newtag.value = pmt::deserialize(sb);
                newtag.srcid = pmt::deserialize(sb);
                tags_out.push_back(newtag);
            }

            return msg.size() - sb.in_avail();
        }

        std::vector<double> rx_combining_impl::compute_llrs(const float *bins_prob, int size, bool use_ldro)
        {
            std::vector<double> LLRs(SF_MAX, 0);
            int sf = std::log2(size);
            for (uint32_t i = 0; i < sf; i++)
            {
                double sum_X1(0), sum_X0(0); // X1 = set of symbols where i-th bit is '1'
                for (uint32_t n = 0; n < m_samples_per_symbol; n++)
                { // for all symbols n : 0 --> 2^sf
                    uint32_t s = mod(n - 1, (1 << m_sf)) / ((use_ldro) ? 4 : 1);
                    s = (s ^ (s >> 1u)); // Gray demap
                    if (s & (1u << i))
                        sum_X1 += bins_prob[n]; // Likelihood
                    else
                        sum_X0 += bins_prob[n];
                }
                // [LSB ... ... MSB 0 0] (number of zeros is sfmax(12) - sf)
                // also clamp to prevent inf and -inf for log(0)
                LLRs[sf - 1 - i] = std::clamp(std::log(sum_X1) - std::log(sum_X0), -LLR_CLAMPING_VALUE, LLR_CLAMPING_VALUE);
            }
            return LLRs;
        }
        /*
         * Our virtual destructor.
         */
        rx_combining_impl::~rx_combining_impl() {}

        void rx_combining_impl::set_sf(int sf)
        { // Set he new sf for the frame
            // debug_print("[rx_combining_impl.cc] new sf received " << sf);

            m_sf = sf;
            m_samples_per_symbol = (uint32_t)(1u << m_sf);
            // m_work_done = false;
            m_upchirp.resize(m_samples_per_symbol);
            m_downchirp.resize(m_samples_per_symbol);

            // FFT demodulation preparations
            m_fft_mag.resize(m_samples_per_symbol);
            m_dechirped.resize(m_samples_per_symbol);
            m_rice_val.resize(m_samples_per_symbol);
            m_rayleigh_val.resize(m_samples_per_symbol);
            m_tmp_prob.resize(m_samples_per_symbol);

            set_min_noutput_items(m_samples_per_symbol);
            set_output_multiple(m_samples_per_symbol);
        }

        int rx_combining_impl::general_work(int noutput_items,
                                            gr_vector_int &ninput_items,
                                            gr_vector_const_void_star &input_items,
                                            gr_vector_void_star &output_items)
        {
            auto out = static_cast<output_type *>(output_items[0]);
            int symbol_to_output = 0;
        
            // do
            // {
                if (m_work_done)
                {
                    free_worker_for_next_work("All payload symbols output");
                    //     debug_print(" WORK done rx_combine", m_identity, "rx_combining", BLUE);
                    //     // empty input buffer
                    //     while (1)
                    //     {
                    //         zmq::poll(m_pollitems, 1, std::chrono::milliseconds(100));
                    //         if ((m_pollitems[0].revents & ZMQ_POLLIN)) // got smth from a synchronizer
                    //         {
                    //             debug_print(" had to clear a message in IPC buffer", m_identity, "rx_combining", BLUE);
                    //             m_sync_worker_addr = s_recv(m_input_socket);
                    //             zmq::recv_result_t msg_size = m_input_socket.recv(m_msg_buff);
                    //         }
                    //         else
                    //         {
                    //             break;
                    //         }
                    //     }
                    //     m_input_socket.close();

                    //     return WORK_DONE;
                }

                // poll inpu
                zmq::poll(m_pollitems, 1, std::chrono::milliseconds(m_force_long_wait?300:1)); // std::chrono::seconds(1)

                if(m_force_long_wait)//use long wait only once after receiving decoded header
                {
                    m_force_long_wait = false;
                }
                if ((m_pollitems[0].revents & ZMQ_POLLIN))                // got smth from a synchronizer
                {
                    int rrh_local_id;
                    auto more = m_input_socket.get(zmq::sockopt::rcvmore);
                    m_sync_worker_addr = s_recv(m_input_socket);
                    zmq::recv_result_t msg_size = m_input_socket.recv(m_msg_buff);
                    // debug_print("[rx_combining_impl.cc] received " << msg_size.value() << "B from sync. worker 0x" << /*string_to_hex*/ (m_sync_worker_addr));

                    if (m_rrh_params.size() == 0 && msg_size == 0) // empty message indicates that no preamble was found
                    {
                        m_work_done = true;
                        m_first_rrh_time = std::chrono::steady_clock::now();
                        m_first_rrh_time_received = true;
                        return 0;
                    }

                    // Check if RRH ID already in rrh list
                    auto it_rrh_info = std::find_if(m_rrh_params.begin(), m_rrh_params.end(), boost::bind(&rrh_param::rrh_id, boost::placeholders::_1) == m_sync_worker_addr);

                    if (m_rrh_params.size() == 0 || it_rrh_info == m_rrh_params.end()) // new rrh connected
                    {
                        for (int i = 0; i < m_rrh_params.size(); i++)
                        {
                            debug_print("Work " << m_work_id << ", Sync worker connected: 0x" << (m_rrh_params[i].rrh_id), m_identity, "rx_combining", BLUE);
                        }
                        debug_print("Work " << m_work_id << ", New sync worker connected: 0x" << (m_sync_worker_addr), m_identity, "rx_combining", BLUE);

                        m_is_new_stream = true;
                        // if (m_rrh_params.size() == 0)
                        // {
                        //     m_first_rrh_time = std::chrono::steady_clock::now();
                        //     m_first_rrh_time_received = true;
                        //     m_waiting = true;
                        // }
                    }
                    else
                    {
                        debug_print("Work " << m_work_id << ", sync worker addr 0x" << /*string_to_hex*/ (m_sync_worker_addr) << ", #in worker list: " << it_rrh_info - m_rrh_params.begin() << " sent more samples", m_identity, "rx_combining", BLUE);
                        rrh_local_id = it_rrh_info - m_rrh_params.begin();
                        m_is_new_stream = false;
                    }

                    uint64_t rcv_offset;
                    m_tags.clear();
                    if (msg_size > 0)
                    {
                        m_consumed_bytes = parse_tag_header(m_msg_buff, rcv_offset, m_tags);
                    }

                    debug_print("Work " << m_work_id << ", tag_parsing: #tags: " << m_tags.size() << " consumed bytes: " << m_consumed_bytes, m_identity, "rx_combining", BLUE);
                    /* Fixup the tags offset to be relative to the start of this message */
                    for (unsigned int i = 0; i < m_tags.size(); i++)
                    {
                        if (m_tags[i].key != pmt::string_to_symbol("frame_info"))
                        {
                            error_print("one tag received: not frame_info, but " << m_tags[i].key, m_identity);
                            continue;
                        }
                        m_tags[i].offset -= rcv_offset;
                        pmt::pmt_t err = pmt::string_to_symbol("error");
                        bool is_header = pmt::to_bool(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("is_header"), err));
                        if (is_header) // new frame beginning from a new rrh
                        {
                            rrh_param new_rrh_param;
                            new_rrh_param.rrh_id = m_sync_worker_addr;
                            int cfo_int = pmt::to_long(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("cfo_int"), err));
                            float cfo_frac = pmt::to_float(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("cfo_frac"), err));
                            new_rrh_param.cfo = cfo_int + cfo_frac;
                            new_rrh_param.noise_pow = pmt::to_float(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("noise_pow"), err));
                            new_rrh_param.sig_pow = pmt::to_float(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("sig_pow"), err));

                            uint16_t new_work_id = pmt::to_long(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("work_id"), err));

                            new_rrh_param.time_full = pmt::to_uint64(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("time_full"), err));
                            new_rrh_param.time_frac = pmt::to_double(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("time_frac"), err));

                            new_rrh_param.buffer_addr = pmt::symbol_to_string(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("buffer_addr"), err));

                            int sf = pmt::to_double(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("sf"), err));
                            m_bw = pmt::to_uint64(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("bandwidth"), err));

                            debug_print("already " << m_rrh_params.size() << " stream, New tag: workID= " << new_work_id << ", buffer addr= " << new_rrh_param.buffer_addr << ", offset= " << m_tags[i].offset << ", sf= " << (int)sf << ", bandwidth= " << m_bw << ", cfo= " << new_rrh_param.cfo << ", Noise var = " << new_rrh_param.noise_pow << ", sig power=" << new_rrh_param.sig_pow, m_identity, "rx_combining", BLUE);
                            if (m_rrh_params.size() == 0)
                            { // first tag from a rrh keep it as the reference for this frame and set work SF
                                if (sf != m_sf)
                                {
                                    set_sf(sf);
                                }

                                if (m_rrh_params.size() == 0)
                                {
                                    m_first_rrh_time = std::chrono::steady_clock::now();

                                    // get current time
                                    m_first_rrh_time_received = true;
                                    m_waiting = true;
                                }

                                m_frame_tag = m_tags[i];
                                m_work_id = new_work_id;
                                m_decoded_header = false;

                                // replace individual info by a vector of dictionaries (one per rrh)
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("buffer_addr"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("cfo_int"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("cfo_frac"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("time_full"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("time_frac"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("sig_pow"));
                                m_frame_tag.value = pmt::dict_delete(m_frame_tag.value, pmt::string_to_symbol("noise_pow"));
                                m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::string_to_symbol("rrh_info_vect"), pmt::make_vector(MAX_COMBINED_RRH, pmt::make_dict()));
                            }
                            if (!m_is_new_stream)
                            {
                                error_print("Only new streams should contain frame info tags!", m_identity);
                            }
                            else if (m_is_new_stream && !m_waiting) // Only consider streams arriving on time
                            {
                                error_print("This new stream arrived too late, consider increasing the wait time RRH_WAIT_MS (in header file)", m_identity);
                            }
                            else if (m_work_id != new_work_id && m_rrh_params.size() != 0)
                            {
                                error_print("new work id while previous is not done. Should not happen! new id " << new_work_id << ", current " << m_work_id, m_identity);
                            }
                            else if (m_rrh_params.size() >= MAX_COMBINED_RRH)
                            {
                                error_print("Already " << m_rrh_params.size() << "/" << MAX_COMBINED_RRH << "involved in combining, can't combine more" << m_work_id, m_identity);
                            }
                            else
                            { // this is a valid new stream

                                rrh_local_id = m_rrh_params.size();
                                // add rrh info to dictionary
                                pmt::pmt_t new_frame_dict = pmt::make_dict();
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("time_frac"), pmt::mp(new_rrh_param.time_frac));
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("time_full"), pmt::mp(new_rrh_param.time_full));
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("sig_pow"), pmt::mp(new_rrh_param.sig_pow));
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("noise_pow"), pmt::mp(new_rrh_param.noise_pow));
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("cfo"), pmt::mp(new_rrh_param.cfo));
                                new_frame_dict = pmt::dict_add(new_frame_dict, pmt::string_to_symbol("buffer_addr"), pmt::mp(new_rrh_param.buffer_addr));

                                pmt::vector_set(pmt::dict_ref(m_frame_tag.value, pmt::string_to_symbol("rrh_info_vect"), err), m_rrh_params.size(), new_frame_dict);

                                // debug_print("added buffer to list: "<<pmt::symbol_to_string(pmt::vector_ref(pmt::dict_ref(m_frame_tag.value, pmt::string_to_symbol("buffer_addr_vect"),err),m_rrh_params.size())));

                                // create downchirp taking CFO_int into account
                                if (&m_upchirp[0] != nullptr)
                                {
                                    build_upchirp(&m_upchirp[0], mod(cfo_int, m_samples_per_symbol), m_sf);
                                }
                                else
                                {
                                    error_print("m_upchirp is null", m_identity);
                                }

                                new_rrh_param.ref_downchirp.resize(m_samples_per_symbol);
                                volk_32fc_conjugate_32fc(&new_rrh_param.ref_downchirp[0], &m_upchirp[0], m_samples_per_symbol);
                                volk_32fc_conjugate_32fc(&m_downchirp[0], &m_upchirp[0], m_samples_per_symbol);

                                // adapt the downchirp to the cfo_frac of the frame
                                for (int n = 0; n < m_samples_per_symbol; n++)
                                {
                                    new_rrh_param.ref_downchirp[n] = new_rrh_param.ref_downchirp[n] * gr_expj(-2 * M_PI * cfo_frac / m_samples_per_symbol * n);
                                }
                                // for (int n = 0; n < m_samples_per_symbol; n++)
                                // {
                                //     m_downchirp[n] = m_downchirp[n] * gr_expj(-2 * M_PI * cfo_frac / m_samples_per_symbol * n);
                                // }

                                m_rrh_params.push_back(new_rrh_param);
                                once_per_stream = true;
                            }
                        }
                        else
                        {
                            long cr = pmt::to_long(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("cr"), err));
                            bool ldro = pmt::to_bool(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("ldro"), err));
                            long symb_numb = pmt::to_long(pmt::dict_ref(m_tags[i].value, pmt::string_to_symbol("symb_numb"), err));
                            error_print(" New tag that isn't a new header frame info: We  souldn't receive that for CRAN!! cr= " << (int)cr << ", ldro= " << ldro, m_identity);
                        }
                    }

                    // check that there is a tag at the beginning of a new rrh stream. If not, consume leftover samples
                    if (m_tags.size() == 0 && m_is_new_stream && m_rrh_params.size() == 0)
                    {
                        debug_print("leftover from previous usage of IPC address", m_identity, "rx_combining", BLUE);
                        m_first_rrh_time_received = false;
                    }
                    else if (m_tags.size() == 0 && m_is_new_stream)
                    {
                        error_print("leftover from previous usage of IPC address? strange as we received other valid streams already, but still possible", m_identity);
                    }
                    else
                    {
                        // store received samples
                        uint32_t to_copy_bytes = (m_msg_buff.size() - m_consumed_bytes);
                        if (to_copy_bytes % sizeof(gr_complex) != 0)
                        {
                            throw std::runtime_error("Incompatible vector sizes: need a multiple of " +
                                                     std::to_string(sizeof(gr_complex)) + " bytes per message");
                        }
                        rx_samples.resize(to_copy_bytes / sizeof(gr_complex));
                        // TODO check how to avoid this double copy/insert
                        memcpy(rx_samples.data(), (uint8_t *)m_msg_buff.data() + m_consumed_bytes, to_copy_bytes);

                        // if(m_rrh_params[rrh_local_id].rx_samples.size()==0 && m_symbols_to_save_per_frame> 0)
                        // {
                        // save downchirp
                        // rx_samples_file.write((char *)m_rrh_params[rrh_local_id].ref_downchirp.data(), m_samples_per_symbol*sizeof(gr_complex));
                        // save samples
                        // rx_samples_file.write((char *)rx_samples.data(), m_symbols_to_save_per_frame*m_samples_per_symbol*sizeof(gr_complex));
                        // }
                        // debug_print("[rx_combining_impl.cc] copy " << rx_samples.size() << "B to rrh stream " << rrh_local_id); //<<" already "<<m_rrh_params[rrh_local_id].rx_samples.size()<<" B");
                        m_rrh_params[rrh_local_id].rx_samples.insert(m_rrh_params[rrh_local_id].rx_samples.end(), rx_samples.begin(), rx_samples.end());
                    }

                    // wait for one RRH to connect to continue
                    if (!m_first_rrh_time_received)
                    {
                        return 0;
                    }

                    // Compute the probabilities for as many symbols as available for each rrh stream received
                    // if (m_rrh_params.size() > 0)//if an we have samples from a RRH
                    int new_symbols_to_proc;
                    for (int rrh_local_id_to_proc = 0; rrh_local_id_to_proc < m_rrh_params.size(); rrh_local_id_to_proc++)
                    {
                        if (m_use_approx_comb)
                        {
                            new_symbols_to_proc = m_rrh_params[rrh_local_id_to_proc].rx_samples.size() / m_samples_per_symbol - m_rrh_params[rrh_local_id_to_proc].symb_prob.size() / m_samples_per_symbol;
                            debug_print("Work " << m_work_id << ", rx_symb= " << m_rrh_params[rrh_local_id_to_proc].rx_samples.size() / m_samples_per_symbol
                                                << ", symb_prob_computed= " << m_rrh_params[rrh_local_id_to_proc].symb_prob.size() / m_samples_per_symbol
                                                << ", new symb to process= " << new_symbols_to_proc,
                                        m_identity, "rx_combining", BLUE);
                            int already_computed_prob = m_rrh_params[rrh_local_id_to_proc].symb_prob.size();
                            for (int i = 0; i < new_symbols_to_proc; i++)
                            {
                                compute_dechirp_fft_mag(&m_rrh_params[rrh_local_id_to_proc].rx_samples[m_samples_per_symbol * i + already_computed_prob],
                                                        &m_rrh_params[rrh_local_id_to_proc].ref_downchirp[0],
                                                        &m_fft_mag[0]);
                                bool valid = compute_rice(&m_fft_mag[0], &m_rice_val[0], m_samples_per_symbol, m_rrh_params[rrh_local_id_to_proc].sig_pow, m_rrh_params[rrh_local_id_to_proc].noise_pow);
                                valid &= compute_scaled_rayleigh(&m_fft_mag[0], &m_rayleigh_val[0], m_samples_per_symbol, m_rrh_params[rrh_local_id_to_proc].noise_pow, m_samples_per_symbol - 1);

                                if (valid)
                                {
                                    // get probability of each bin
                                    volk_32f_x2_add_32f(m_tmp_prob.data(), m_rice_val.data(), m_rayleigh_val.data(), m_samples_per_symbol);
                                    volk_32f_x2_divide_32f(m_tmp_prob.data(), m_rice_val.data(), m_tmp_prob.data(), m_samples_per_symbol);
                                }
                                else // just use the max bin
                                {
                                    if (once_per_stream == true)
                                    {
                                        warning_print("overflow in probability computation, just use Argmax of bins", m_identity);
                                        once_per_stream = false;
                                    }
                                    int argmax_index = std::distance(m_fft_mag.data(), std::max_element(m_fft_mag.data(), m_fft_mag.data() + m_samples_per_symbol));
                                    for (int j = 0; j < m_samples_per_symbol; j++)
                                    {
                                        if (j == argmax_index)
                                            m_tmp_prob[j] = 1;
                                        else
                                            m_tmp_prob[j] = 1e-9;
                                    }
                                }
                                m_rrh_params[rrh_local_id_to_proc].symb_prob.insert(m_rrh_params[rrh_local_id_to_proc].symb_prob.end(), m_tmp_prob.begin(), m_tmp_prob.end());

                            }
                        }
                        else
                        {
                            // store rician and rayleigh values for later combining
                            new_symbols_to_proc = m_rrh_params[rrh_local_id_to_proc].rx_samples.size() / m_samples_per_symbol - m_rrh_params[rrh_local_id_to_proc].rice_val.size() / m_samples_per_symbol;
                            debug_print("Work " << m_work_id << ", rx_symb= " << m_rrh_params[rrh_local_id_to_proc].rx_samples.size() / m_samples_per_symbol
                                                << ", symb_prob_computed= " << m_rrh_params[rrh_local_id_to_proc].symb_prob.size() / m_samples_per_symbol
                                                << ", new symb to process= " << new_symbols_to_proc,
                                        m_identity, "rx_combining", BLUE);
                            int already_computed_samples = m_rrh_params[rrh_local_id_to_proc].rice_val.size();
                            for (int i = 0; i < new_symbols_to_proc; i++)
                            {
                                compute_dechirp_fft_mag(&m_rrh_params[rrh_local_id_to_proc].rx_samples[m_samples_per_symbol * i + already_computed_samples],
                                                        &m_rrh_params[rrh_local_id_to_proc].ref_downchirp[0],
                                                        &m_fft_mag[0]);
                                compute_rice(&m_fft_mag[0], &m_rice_val[0], m_samples_per_symbol, m_rrh_params[rrh_local_id_to_proc].sig_pow, m_rrh_params[rrh_local_id_to_proc].noise_pow);
                                compute_scaled_rayleigh(&m_fft_mag[0], &m_rayleigh_val[0], m_samples_per_symbol, m_rrh_params[rrh_local_id_to_proc].noise_pow, 1);

                                m_rrh_params[rrh_local_id_to_proc].rice_val.insert(m_rrh_params[rrh_local_id_to_proc].rice_val.end(), m_rice_val.begin(), m_rice_val.end());
                                m_rrh_params[rrh_local_id_to_proc].rayleigh_val.insert(m_rrh_params[rrh_local_id_to_proc].rayleigh_val.end(), m_rayleigh_val.begin(), m_rayleigh_val.end());
                            }
                        }
                    }
                }
                // wait for one RRH to connect to continue
                if (!m_first_rrh_time_received)
                {
                    return 0;
                }
                // wait for additional RRHs if not enough time passed since first stream of samples
                auto now = std::chrono::steady_clock::now();
                auto delta = now - m_first_rrh_time;

                debug_print("Work " << m_work_id << ", wait counted: " << delta / std::chrono::milliseconds(1) << "/" << std::chrono::milliseconds(RRH_WAIT_MS) / std::chrono::milliseconds(1), m_identity, "rx_combining", BLUE);
                if (delta < std::chrono::milliseconds(RRH_WAIT_MS))
                {
                    return 0;
                }
                else
                {
                    m_waiting = false;
                }
                if (m_rrh_params.size() == 0) // no valid rrh stream received on time
                {
                    free_worker_for_next_work("No valid RRH stream received on time");
                    return 0;
                }

                // compute the number of symbol probabilities that can be combined (minimum number of symbol proba available from all RRHs)
                int n_symbols_ready = m_rrh_params.size() > 0 ? INT_MAX : 0;
                int n_symbols_ready_rrh;
                for (int rrh_local_id_to_proc = 0; rrh_local_id_to_proc < m_rrh_params.size(); rrh_local_id_to_proc++)
                {
                    if (m_use_approx_comb)
                    {
                        n_symbols_ready_rrh = (m_rrh_params[rrh_local_id_to_proc].symb_prob.size() / m_samples_per_symbol) - numb_processed_symb;
                    }
                    else
                    {
                        n_symbols_ready_rrh = (m_rrh_params[rrh_local_id_to_proc].rice_val.size() / m_samples_per_symbol) - numb_processed_symb;
                    }
                    n_symbols_ready = std::min(n_symbols_ready, n_symbols_ready_rrh);
                }
                debug_print("Work " << m_work_id << ", uses " << m_rrh_params.size() << " rrhs, " << n_symbols_ready << " new symbols ready to combine", m_identity, "rx_combining", BLUE);
                debug_print("m_output_symbol_count " << m_output_symbol_count << ", m_decoded_header " << m_decoded_header << " m_symb_numb " << m_symb_numb, m_identity, "rx_combining", BLUE);

                // log frame info at each RRH [n_rrh_combined, buffer_addr, time_full, time_frac, snr_est, is_last_rrh]
                if (numb_processed_symb == 0)
                {
                    // for(int i = 0; i<m_rrh_params.size();i++){
                    //     float snr_est = m_rrh_params[i].sig_pow/m_rrh_params[i].noise_pow;
                    //     if(i<m_rrh_params.size()-1){
                    //         log_time_file <<m_rrh_params.size()<<","<< m_rrh_params[i].buffer_addr << ","<< m_rrh_params[i].time_full<<","<<std::setprecision(9)<<std::fixed <<m_rrh_params[i].time_frac<<std::defaultfloat<<","<<snr_est<<","<<0<<std::endl;
                    //     }
                    //     else{//last rrh
                    //         log_time_file <<m_rrh_params.size()<<","<< m_rrh_params[i].buffer_addr << ","<< m_rrh_params[i].time_full<<","<<std::setprecision(9)<<std::fixed <<m_rrh_params[i].time_frac<<std::defaultfloat<<","<<snr_est<<","<<1<<std::endl;
                    //     }
                    // }
                }

                // combine into probabilities
                for (int i = 0; i < n_symbols_ready; i++)
                {
                    if (m_output_symbol_count == 8 && !m_decoded_header) // we need to wait for the header to be decoded
                    {
                        break;
                    }
                    // else if (m_decoded_header && m_output_symbol_count == m_symb_numb) // we are done with this frame
                    // {
                    //     m_work_done = true;
                    //     break;
                    // }
                    for (int rrh_local_id_to_proc = 0; rrh_local_id_to_proc < m_rrh_params.size(); rrh_local_id_to_proc++)
                    {
                        if (m_use_approx_comb)
                        {
                            if (rrh_local_id_to_proc == 0)
                            {
                                memcpy(m_tmp_prob.data(), &m_rrh_params[rrh_local_id_to_proc].symb_prob[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol * sizeof(float));
                            }
                            else
                            {
                                volk_32f_x2_multiply_32f(m_tmp_prob.data(), m_tmp_prob.data(), &m_rrh_params[rrh_local_id_to_proc].symb_prob[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol);
                            }
                        }
                        else
                        { // compute rayleigh and rice combined parts
                            if (rrh_local_id_to_proc == 0)
                            {
                                memcpy(m_rice_val.data(), &m_rrh_params[rrh_local_id_to_proc].rice_val[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol * sizeof(float));
                                memcpy(m_rayleigh_val.data(), &m_rrh_params[rrh_local_id_to_proc].rayleigh_val[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol * sizeof(float));
                            }
                            else
                            {
                                volk_32f_x2_multiply_32f(m_rice_val.data(), m_rice_val.data(), &m_rrh_params[rrh_local_id_to_proc].rice_val[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol);
                                volk_32f_x2_multiply_32f(m_rayleigh_val.data(), m_rayleigh_val.data(), &m_rrh_params[rrh_local_id_to_proc].rayleigh_val[numb_processed_symb * m_samples_per_symbol], m_samples_per_symbol);
                            }
                        }
                    }

                    if (!m_use_approx_comb)
                    { // get proba from the combined rayleigh and rice
                        volk_32f_s32f_multiply_32f(m_tmp_prob.data(), m_rayleigh_val.data(), m_samples_per_symbol - 1, m_samples_per_symbol);
                        volk_32f_x2_add_32f(m_tmp_prob.data(), m_tmp_prob.data(), m_rice_val.data(), m_samples_per_symbol);
                        volk_32f_x2_divide_32f(m_tmp_prob.data(), m_rice_val.data(), m_tmp_prob.data(), m_samples_per_symbol);
                    }

                    // compute LLRs of each bit
                    std::vector<double> llrs = compute_llrs(m_tmp_prob.data(), m_samples_per_symbol, m_output_symbol_count < 8 || m_ldro);
                    

                    // output LLRS
                    memcpy(&out[i * SF_MAX], llrs.data(), SF_MAX * sizeof(double));

                    if (m_output_symbol_count == 0)
                    {
                        // add tag to indicate new frame
                        debug_print("Work " << m_work_id << ", added a new frame tag", m_identity, "rx_combining", BLUE);
                        m_frame_tag.value = pmt::dict_add(m_frame_tag.value, pmt::string_to_symbol("n_rrh_involved"), pmt::from_long(m_rrh_params.size()));
                        m_frame_tag.offset = nitems_written(0);
                        add_item_tag(0, m_frame_tag);
                    }
                    numb_processed_symb++;
                    m_output_symbol_count++;
                    symbol_to_output++;
                    if (m_decoded_header && m_output_symbol_count == m_symb_numb) // we are done with this frame
                    {
                        m_work_done = true;
                        break;
                    }
                }

                // Tell runtime system how many output items we produced.
                if (symbol_to_output > 0)
                {
                    debug_print("Work " << m_work_id << ", outputs " << symbol_to_output << ", total= " << m_output_symbol_count << ", over " << m_symb_numb << ", nitemsWritten= " << nitems_written(0), m_identity, "rx_combining", BLUE);
                }
            // } while (symbol_to_output == 0 && m_first_rrh_time_received && m_decoded_header);
            return symbol_to_output;
        }

    } /* namespace cran */
} /* namespace gr */
