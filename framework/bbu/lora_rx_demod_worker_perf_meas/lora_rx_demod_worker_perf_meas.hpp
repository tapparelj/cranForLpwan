#ifndef LORA_RX_DEMOD_WORKER_HPP
#define LORA_RX_DEMOD_WORKER_HPP
/********************
GNU Radio C++ Flow Graph Header File

Title: lora_rx_demod_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.8.2.0
********************/

/********************
** Create includes
********************/
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/file_sink.h>
#include "gnuradio/cran/utilities.h"
#include <gnuradio/blocks/tag_debug.h>
#include <gnuradio/zeromq/push_msg_sink.h>
#include "crc_verif.h"
#include "deinterleaver.h"
#include "dewhitening.h"
#include "gray_mapping.h"
#include "hamming_dec.h"
#include "header_decoder.h"
#include "rx_combining.h"
#include "worker_zmq_sink.h"



using namespace gr;

class lora_rx_demod_worker_perf_meas {

private:
    std::vector<lora_sdr::header_decoder::sptr> lora_sdr_header_decoder;
    std::vector<lora_sdr::hamming_dec::sptr> lora_sdr_hamming_dec;
    std::vector<lora_sdr::gray_mapping::sptr> lora_sdr_gray_mapping;
    std::vector<lora_sdr::dewhitening::sptr> lora_sdr_dewhitening;
    std::vector<lora_sdr::deinterleaver::sptr> lora_sdr_deinterleaver;
    std::vector<lora_sdr::crc_verif::sptr> lora_sdr_crc_verif;
    cran::rx_combining::sptr cran_rx_combining_0;
    std::vector<cran::worker_zmq_sink::sptr> worker_zmq_sink;
//----debug
    // blocks::tag_debug::sptr tag_debug_0;
    blocks::file_sink::sptr blocks_file_sink_0;
//----



// Variables:
    int n_rrh = 4;
    int pay_len = 11;
    bool impl_head = false;
    bool has_crc = false;
    int cr = 0;
    double center_freq = 868.1e6;
    int bw = 125000;
    bool print_head = true;
    bool soft_dec = true;
    int sf = 7;
    bool max_log_approx = false;
    uint8_t ldro_mode = 0; //0 disable, 1:enabled, 2: auto (enabled for Tsymb>16ms)
    uint8_t os_factor = 4;

public:

    top_block_sptr tb;
    lora_rx_demod_worker_perf_meas(std::string broker_addr, std::string comb_input_addr, std::string app_addr);
    ~lora_rx_demod_worker_perf_meas();

    uint8_t get_os_factor () const;
    void set_os_factor (uint8_t os_factor);
    int get_pay_len () const;
    void set_pay_len(int pay_len);
    bool get_impl_head () const;
    void set_impl_head(bool impl_head);
    bool get_has_crc () const;
    void set_has_crc(bool has_crc);
    int get_cr () const;
    void set_cr(int cr);
    double get_center_freq () const;
    void set_center_freq(double center_freq);
    int get_bw () const;
    void set_bw(int bw);
};


#endif

