/********************
GNU Radio C++ Flow Graph Source File

Title: lora_rx_sync_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

#include "lora_rx_sync_worker.hpp"
using namespace gr;

lora_rx_sync_worker::lora_rx_sync_worker(std::string broker_addr)
{

    this->tb = gr::make_top_block("lora_rx_sync_worker");

    // Blocks:
    {
        this->cran_lora_frame_sync_0 = cran::lora_frame_sync::make(center_freq, bw, sf, impl_head, {8, 16}, os_factor, 8);
    }
    {
        this->cran_worker_zmq_source_0 = cran::worker_zmq_source::make(broker_addr);
    }
    {
        this->zmq_dealer_sink_0 = cran::zmq_dealer_sink::make();
    }
    this->delay_0 = blocks::delay::make(sizeof(gr_complex), 25*(1u<<12)*os_factor);//to prefill input buffer

    std::vector<float> filter_taps = filter::firdes::low_pass_2(1, bw*os_factor, bw / 2*1.1, 10e3, 30, fft::window::WIN_HAMMING);
    this->interp_fir_filter_0 = filter::interp_fir_filter<gr_complex, gr_complex, float>::make(os_factor/(rx_sampling_rate/bw), filter_taps);
    this->interp_fir_filter_0->set_min_output_buffer((int)((1u << 12)*os_factor * 25));
    this->tb->hier_block2::msg_connect(this->cran_lora_frame_sync_0, "preamb_info", this->cran_worker_zmq_source_0, "preamb_info");
    
    this->tb->hier_block2::msg_connect(this->cran_worker_zmq_source_0, "system_out", this->zmq_dealer_sink_0, "work_done");

    this->tb->hier_block2::connect(this->cran_worker_zmq_source_0, 0, this->delay_0, 0);
    this->tb->hier_block2::connect(this->delay_0, 0, this->interp_fir_filter_0, 0);
    this->tb->hier_block2::connect(this->interp_fir_filter_0, 0, this->cran_lora_frame_sync_0, 0);
    this->tb->hier_block2::connect(this->cran_lora_frame_sync_0, 0, this->zmq_dealer_sink_0, 0);
}

lora_rx_sync_worker::~lora_rx_sync_worker()
{
}

// Callbacks:

uint8_t lora_rx_sync_worker::get_os_factor() const
{
    return this->os_factor;
}

void lora_rx_sync_worker::set_os_factor(uint8_t os_factor)
{
    this->os_factor = os_factor;
}

int lora_rx_sync_worker::get_pay_len() const
{
    return this->pay_len;
}

void lora_rx_sync_worker::set_pay_len(int pay_len)
{
    this->pay_len = pay_len;
}

bool lora_rx_sync_worker::get_impl_head() const
{
    return this->impl_head;
}

void lora_rx_sync_worker::set_impl_head(bool impl_head)
{
    this->impl_head = impl_head;
}

bool lora_rx_sync_worker::get_has_crc() const
{
    return this->has_crc;
}

void lora_rx_sync_worker::set_has_crc(bool has_crc)
{
    this->has_crc = has_crc;
}

int lora_rx_sync_worker::get_cr() const
{
    return this->cr;
}

void lora_rx_sync_worker::set_cr(int cr)
{
    this->cr = cr;
}

double lora_rx_sync_worker::get_center_freq() const
{
    return this->center_freq;
}

void lora_rx_sync_worker::set_center_freq(double center_freq)
{
    this->center_freq = center_freq;
}

int lora_rx_sync_worker::get_bw() const
{
    return this->bw;
}

void lora_rx_sync_worker::set_bw(int bw)
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
