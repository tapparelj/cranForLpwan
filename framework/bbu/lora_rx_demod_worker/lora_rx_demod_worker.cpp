/********************
GNU Radio C++ Flow Graph Source File

Title: lora_rx_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

#include "lora_rx_demod_worker.hpp"
using namespace gr;

lora_rx_demod_worker::lora_rx_demod_worker(std::string broker_addr, std::string comb_input_addr, std::string app_addr)
{

    this->tb = gr::make_top_block("lora_rx_demod_worker");

    // Blocks:
    {
        this->lora_sdr_header_decoder_0 = lora_sdr::header_decoder::make(false, 0, 255, 1, ldro_mode, print_head);
    }
    {
        this->lora_sdr_hamming_dec_0 = lora_sdr::hamming_dec::make(soft_dec);
    }
    {
        this->lora_sdr_gray_mapping_0 = lora_sdr::gray_mapping::make(soft_dec);
    }
    {
        this->cran_rx_combining_0 = cran::rx_combining::make(comb_input_addr);
        this->cran_rx_combining_0->set_min_output_buffer(1u<<13);
    }
    {
        this->lora_sdr_dewhitening_0 = lora_sdr::dewhitening::make();
    }
    {
        this->lora_sdr_deinterleaver_0 = lora_sdr::deinterleaver::make(soft_dec);
    }
    {
        this->lora_sdr_crc_verif_0 = lora_sdr::crc_verif::make(true, false);
    }
    {
        this->worker_zmq_sink_0 = cran::worker_zmq_sink::make(broker_addr, app_addr);
    }

    this->tb->hier_block2::connect(this->cran_rx_combining_0, 0, this->lora_sdr_gray_mapping_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_gray_mapping_0, 0, this->lora_sdr_deinterleaver_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_deinterleaver_0, 0, this->lora_sdr_hamming_dec_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_hamming_dec_0, 0, this->lora_sdr_header_decoder_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_header_decoder_0, 0, this->lora_sdr_dewhitening_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_dewhitening_0, 0, this->lora_sdr_crc_verif_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_crc_verif_0, 0, this->worker_zmq_sink_0, 0);
    // this->tb->hier_block2::msg_connect(this->lora_sdr_crc_verif_0, "msg", this->worker_zmq_sink_0, "msg");
    this->tb->hier_block2::msg_connect(this->lora_sdr_header_decoder_0, "frame_info", this->cran_rx_combining_0, "frame_info");
}

lora_rx_demod_worker::~lora_rx_demod_worker()
{
}