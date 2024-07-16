#ifndef LORA_RX_WORKER_HPP
#define LORA_RX_WORKER_HPP
/********************
GNU Radio C++ Flow Graph Header File

Title: lora_rx_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.8.2.0
********************/

/********************
** Create includes
********************/
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/file_sink.h>
#include "gnuradio/cran/utilities.h"
#include <gnuradio/filter/api.h>
#include <gnuradio/blocks/tag_debug.h>
#include <gnuradio/filter/fir_filter.h>
#include <gnuradio/filter/interp_fir_filter.h>
#include <gnuradio/zeromq/push_msg_sink.h>
#include <gnuradio/filter/firdes.h>
#include "worker_zmq_source.h"
#include "crc_verif.h"
#include "deinterleaver.h"
#include "dewhitening.h"
#include "fft_demod.h"
#include "frame_sync.h"
#include "gray_mapping.h"
#include "hamming_dec.h"
#include "header_decoder.h"



using namespace gr;


// static uint worker_idx = 0;
class lora_rx_worker {

private:
    lora_sdr::header_decoder::sptr lora_sdr_header_decoder_0;
    lora_sdr::hamming_dec::sptr lora_sdr_hamming_dec_0;
    lora_sdr::gray_mapping::sptr lora_sdr_gray_mapping_0;
    lora_sdr::frame_sync::sptr lora_sdr_frame_sync_0;
    lora_sdr::fft_demod::sptr lora_sdr_fft_demod_0;
    lora_sdr::dewhitening::sptr lora_sdr_dewhitening_0;
    lora_sdr::deinterleaver::sptr lora_sdr_deinterleaver_0;
    lora_sdr::crc_verif::sptr lora_sdr_crc_verif_0;
    cran::worker_zmq_source::sptr cran_worker_zmq_source_0;
    zeromq::push_msg_sink::sptr zmq_push_msg_sink_0;
    filter::interp_fir_filter<gr_complex,gr_complex,float>::sptr interp_fir_filter_0;
//----debug
    // blocks::tag_debug::sptr tag_debug_0;
    blocks::file_sink::sptr blocks_file_sink_0;

    //----



// Variables:
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
    uint8_t ldro_mode = 2; //0 disable, 1:enabled, 2: auto (enabled for Tsymb>16ms)
    uint8_t os_factor = 4;
    //for debug

public:

    top_block_sptr tb;
    lora_rx_worker(std::string broker_addr, std::string app_addr);
    ~lora_rx_worker();

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

