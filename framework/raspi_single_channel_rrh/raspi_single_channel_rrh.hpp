#ifndef SINGLE_CHANNEL_RRH_HPP
#define SINGLE_CHANNEL_RRH_HPP
/********************
GNU Radio C++ Flow Graph Header File

Title: rrh
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

/********************
** Create includes
********************/
#include <gnuradio/top_block.h>
#include <gnuradio/blocks/multiply.h>
#include <gnuradio/blocks/tag_debug.h>
#include <gnuradio/blocks/keep_one_in_n.h>
#include <gnuradio/zeromq/push_msg_sink.h>
#include "rrh_zmq_sink.h"
#include "lora_frame_detector.h"
#include "lora_frame_detector_single_sf.h"
#include "uhd_usrp_source_custom.h"
#include "../config.hpp"
#include "gnuradio/cran/utilities.h"
#include "gnuradio/cran/zhelper.h"
#include <gnuradio/filter/fir_filter_blk.h>
#include <gnuradio/analog/sig_source.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/mmse_resampler_cc.h>
#include <gnuradio/soapy/source.h>
#include <gnuradio/uhd/usrp_source.h>
#include <chrono>
#include <ctime>  
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <libserial/SerialPort.h>
#include <libserial/SerialStream.h>
#include <memory>
#include <string.h>

using namespace gr;
using namespace LibSerial ;

class raspi_single_channel_rrh
{
private:
    char m_identity[10] = {}; //unique identifier
    cran::lora_frame_detector_single_sf::sptr cran_lora_frame_detector_0;
    cran::rrh_zmq_sink::sptr cran_rrh_zmq_sink_0;
    cran::uhd_usrp_source_custom::sptr uhd_usrp_source_0;
    soapy::source::sptr soapy_source_0;
    

    blocks::tag_debug::sptr tag_debug_0;
    blocks::keep_one_in_n::sptr keep_one_in_n_0;
    
    
    // Variables:    
    int os_factor = 2;
    int center_freq = cran::center_freq_MHz[config.rrh_channel_indices[0]] * 1e6; //Only listen to first channel
    int bw = 125e3;
    int sample_rate = bw*os_factor;
    int m_sf = 7;
    
    float rx_gain = 20; //0-31 for USRP, -12-64 for LimeSDR
    
    std::string usrp_addr="";
    
    bool is_usrp = true; //false for limesdr 
    bool use_GPS = true; //only available for USRP

public:
    top_block_sptr tb;
    raspi_single_channel_rrh(int sf);
    ~raspi_single_channel_rrh();
};

#endif
