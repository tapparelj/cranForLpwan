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

class lora_rx_demod_worker {

private:
    lora_sdr::header_decoder::sptr lora_sdr_header_decoder_0;
    lora_sdr::hamming_dec::sptr lora_sdr_hamming_dec_0;
    lora_sdr::gray_mapping::sptr lora_sdr_gray_mapping_0;
    lora_sdr::dewhitening::sptr lora_sdr_dewhitening_0;
    lora_sdr::deinterleaver::sptr lora_sdr_deinterleaver_0;
    lora_sdr::crc_verif::sptr lora_sdr_crc_verif_0;
    cran::rx_combining::sptr cran_rx_combining_0;
    cran::worker_zmq_sink::sptr worker_zmq_sink_0;
//----debug
    blocks::tag_debug::sptr tag_debug_0;
    blocks::file_sink::sptr blocks_file_sink_0;
//----



// Variables:
    bool print_head = true;
    bool soft_dec = true;
    bool max_log_approx = false;
    uint8_t ldro_mode = 0; //0 disable, 1:enabled, 2: auto (enabled for Tsymb>16ms)

public:

    top_block_sptr tb;
    lora_rx_demod_worker(std::string broker_addr, std::string comb_input_addr, std::string app_addr);
    ~lora_rx_demod_worker();
};


#endif

