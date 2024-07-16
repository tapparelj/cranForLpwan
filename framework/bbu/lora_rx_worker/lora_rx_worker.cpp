/********************
GNU Radio C++ Flow Graph Source File

Title: lora_rx_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

#include "lora_rx_worker.hpp"
using namespace gr;

lora_rx_worker::lora_rx_worker(std::string broker_addr, std::string app_addr)
{

    this->tb = gr::make_top_block("lora_rx_worker");

    // Blocks:
    {
        this->lora_sdr_header_decoder_0 = lora_sdr::header_decoder::make(impl_head, cr, pay_len, has_crc, ldro_mode, print_head);
    }
    {
        this->lora_sdr_hamming_dec_0 = lora_sdr::hamming_dec::make(soft_dec);
    }
    {
        this->lora_sdr_gray_mapping_0 = lora_sdr::gray_mapping::make(soft_dec);
    }
    {
        this->lora_sdr_frame_sync_0 = lora_sdr::frame_sync::make(center_freq, bw, sf, impl_head, {8, 16}, os_factor, 8);
    }
    {
        this->lora_sdr_fft_demod_0 = lora_sdr::fft_demod::make(soft_dec, max_log_approx);
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
        this->cran_worker_zmq_source_0 = cran::worker_zmq_source::make(broker_addr);
    }
    {
        this->zmq_push_msg_sink_0 = zeromq::push_msg_sink::make((char *)app_addr.c_str(), 100, false);
    }

    std::vector<float> filter_taps = filter::firdes::low_pass_2(1, bw*os_factor, bw / 2+3e3, 10e3, 30, fft::window::WIN_HAMMING);
    this->interp_fir_filter_0 = filter::interp_fir_filter<gr_complex, gr_complex, float>::make(os_factor, filter_taps);
    this->interp_fir_filter_0->set_min_output_buffer((int)((1u << 12) * os_factor * 1.1));

    this->tb->hier_block2::msg_connect(this->lora_sdr_header_decoder_0, "frame_info", this->lora_sdr_frame_sync_0, "frame_info");
    this->tb->hier_block2::msg_connect(this->lora_sdr_frame_sync_0, "preamb_info", this->cran_worker_zmq_source_0, "preamb_info");
    this->tb->hier_block2::msg_connect(this->lora_sdr_header_decoder_0, "frame_info", this->cran_worker_zmq_source_0, "frame_end");
    this->tb->hier_block2::msg_connect(this->lora_sdr_crc_verif_0, "msg", this->cran_worker_zmq_source_0, "frame_end");
    this->tb->hier_block2::msg_connect(this->lora_sdr_crc_verif_0, "msg", this->zmq_push_msg_sink_0, "in");

    this->tb->hier_block2::connect(this->cran_worker_zmq_source_0, 0, this->interp_fir_filter_0, 0);
    this->tb->hier_block2::connect(this->interp_fir_filter_0, 0, this->lora_sdr_frame_sync_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_deinterleaver_0, 0, this->lora_sdr_hamming_dec_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_dewhitening_0, 0, this->lora_sdr_crc_verif_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_fft_demod_0, 0, this->lora_sdr_gray_mapping_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_frame_sync_0, 0, this->lora_sdr_fft_demod_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_gray_mapping_0, 0, this->lora_sdr_deinterleaver_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_hamming_dec_0, 0, this->lora_sdr_header_decoder_0, 0);
    this->tb->hier_block2::connect(this->lora_sdr_header_decoder_0, 0, this->lora_sdr_dewhitening_0, 0);
}

lora_rx_worker::~lora_rx_worker()
{
}

// Callbacks:

uint8_t lora_rx_worker::get_os_factor() const
{
    return this->os_factor;
}

void lora_rx_worker::set_os_factor(uint8_t os_factor)
{
    this->os_factor = os_factor;
}

int lora_rx_worker::get_pay_len() const
{
    return this->pay_len;
}

void lora_rx_worker::set_pay_len(int pay_len)
{
    this->pay_len = pay_len;
}

bool lora_rx_worker::get_impl_head() const
{
    return this->impl_head;
}

void lora_rx_worker::set_impl_head(bool impl_head)
{
    this->impl_head = impl_head;
}

bool lora_rx_worker::get_has_crc() const
{
    return this->has_crc;
}

void lora_rx_worker::set_has_crc(bool has_crc)
{
    this->has_crc = has_crc;
}

int lora_rx_worker::get_cr() const
{
    return this->cr;
}

void lora_rx_worker::set_cr(int cr)
{
    this->cr = cr;
}

double lora_rx_worker::get_center_freq() const
{
    return this->center_freq;
}

void lora_rx_worker::set_center_freq(double center_freq)
{
    this->center_freq = center_freq;
}

int lora_rx_worker::get_bw() const
{
    return this->bw;
}

void lora_rx_worker::set_bw(int bw)
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
