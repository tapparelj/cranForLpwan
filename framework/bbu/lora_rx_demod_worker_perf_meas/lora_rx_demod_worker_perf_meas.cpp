/********************
GNU Radio C++ Flow Graph Source File

Title: lora_rx_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

#include "lora_rx_demod_worker_perf_meas.hpp"
using namespace gr;

lora_rx_demod_worker_perf_meas::lora_rx_demod_worker_perf_meas(std::string broker_addr, std::string comb_input_addr, std::string app_addr)
{

    this->tb = gr::make_top_block("lora_rx_demod_worker");

    for (int i = 0; i < n_rrh+1; i++)//spawn one receiver chain per RRH + one for the combining
    {
        // Blocks:
        {
            this->lora_sdr_header_decoder.push_back(lora_sdr::header_decoder::make(impl_head, cr, pay_len, has_crc, ldro_mode, print_head));
        }
        {
            this->lora_sdr_hamming_dec.push_back(lora_sdr::hamming_dec::make(soft_dec));
        }
        {
            this->lora_sdr_gray_mapping.push_back(lora_sdr::gray_mapping::make(soft_dec));
        }
        {
            this->lora_sdr_dewhitening.push_back(lora_sdr::dewhitening::make());
        }
        {
            this->lora_sdr_deinterleaver.push_back(lora_sdr::deinterleaver::make(soft_dec));
        }
        {
            this->lora_sdr_crc_verif.push_back(lora_sdr::crc_verif::make(true, false));
        }
        {
            this->worker_zmq_sink.push_back(cran::worker_zmq_sink::make(broker_addr, app_addr));
        }
    }
    {
        this->cran_rx_combining_0 = (cran::rx_combining::make(comb_input_addr));
        this->cran_rx_combining_0->set_min_output_buffer(1u<<13);
    }
   

    // this->tb->hier_block2::connect(this->cran_rx_combining_0, 0, this->lora_sdr_gray_mapping_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_gray_mapping_0, 0, this->lora_sdr_deinterleaver_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_deinterleaver_0, 0, this->lora_sdr_hamming_dec_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_hamming_dec_0, 0, this->lora_sdr_header_decoder_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_header_decoder_0, 0, this->lora_sdr_dewhitening_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_dewhitening_0, 0, this->lora_sdr_crc_verif_0, 0);
    // this->tb->hier_block2::connect(this->lora_sdr_crc_verif_0, 0, this->worker_zmq_sink_0, 0);
    // // this->tb->hier_block2::msg_connect(this->lora_sdr_crc_verif_0, "msg", this->worker_zmq_sink_0, "msg");
    // this->tb->hier_block2::msg_connect(this->lora_sdr_header_decoder_0, "frame_info", this->cran_rx_combining_0, "frame_info");

}

lora_rx_demod_worker_perf_meas::~lora_rx_demod_worker_perf_meas()
{
}

// Callbacks:

uint8_t lora_rx_demod_worker_perf_meas::get_os_factor() const
{
    return this->os_factor;
}

void lora_rx_demod_worker_perf_meas::set_os_factor(uint8_t os_factor)
{
    this->os_factor = os_factor;
}

int lora_rx_demod_worker_perf_meas::get_pay_len() const
{
    return this->pay_len;
}

void lora_rx_demod_worker_perf_meas::set_pay_len(int pay_len)
{
    this->pay_len = pay_len;
}

bool lora_rx_demod_worker_perf_meas::get_impl_head() const
{
    return this->impl_head;
}

void lora_rx_demod_worker_perf_meas::set_impl_head(bool impl_head)
{
    this->impl_head = impl_head;
}

bool lora_rx_demod_worker_perf_meas::get_has_crc() const
{
    return this->has_crc;
}

void lora_rx_demod_worker_perf_meas::set_has_crc(bool has_crc)
{
    this->has_crc = has_crc;
}

int lora_rx_demod_worker_perf_meas::get_cr() const
{
    return this->cr;
}

void lora_rx_demod_worker_perf_meas::set_cr(int cr)
{
    this->cr = cr;
}

double lora_rx_demod_worker_perf_meas::get_center_freq() const
{
    return this->center_freq;
}

void lora_rx_demod_worker_perf_meas::set_center_freq(double center_freq)
{
    this->center_freq = center_freq;
}

int lora_rx_demod_worker_perf_meas::get_bw() const
{
    return this->bw;
}

void lora_rx_demod_worker_perf_meas::set_bw(int bw)
{
    this->bw = bw;
}

// int main (int argc, char **argv) {

//     lora_rx_worker* top_block = new lora_rx_worker();
//     top_block->tb->start();
//     std::cout << "Press Enter to quit: ";
//     std::cin.ignore();
//     top_block->tb->stop();
//     top_block->tb->wait();

//     return 0;
// }
