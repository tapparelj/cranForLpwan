/********************
GNU Radio C++ Flow Graph Source File

Title: lora_rx_worker
Author: Tapparel Joachim@TCL,EPFL
GNU Radio version: 3.10.3.0
********************/

#include "raspi_single_channel_rrh.hpp"
using namespace gr;

raspi_single_channel_rrh::raspi_single_channel_rrh(int sf)
{
    this->tb = gr::make_top_block("raspi_single_channel_rrh");

    sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));
    
    m_sf = sf;
    debug_print("RRH config config:",m_identity,"rrh",YELLOW);
    debug_print("center_freq: " << center_freq,m_identity,"rrh",YELLOW);
    debug_print("sample_rate: " << sample_rate,m_identity,"rrh",YELLOW);
    debug_print("bw: " << bw,m_identity,"rrh",YELLOW);
    debug_print("Spreading factor: " << m_sf,m_identity,"rrh",YELLOW);
    
    // Blocks:
    if(is_usrp){      
    //---------- USRP ------------
        this->uhd_usrp_source_0 = cran::uhd_usrp_source_custom::make(
        ::uhd::device_addr_t(""),
        ::uhd::stream_args_t("fc32", ""));
        this->uhd_usrp_source_0->set_samp_rate(sample_rate);
        this->uhd_usrp_source_0->set_center_freq(center_freq, 0);
        this->uhd_usrp_source_0->set_gain(rx_gain, 0);
        this->uhd_usrp_source_0->set_antenna("TX/RX", 0);
        this->uhd_usrp_source_0->set_bandwidth(sample_rate, 0);        
        //::uhd::time_spec_t now = this->uhd_usrp_source_0->get_time_now();
        //// get current time
        //std::chrono::system_clock::now();
        //debug_print("clk cfg: "
         //        << "now: " << now.get_full_secs() << " " << now.get_frac_secs(),m_identity,"rrh",YELLOW);
                 
        this->uhd_usrp_source_0->set_min_output_buffer((int)((1u << m_sf)*sample_rate/bw*20));
    }
    else{
        //---------- Lime SDR Mini -------------
        std::string device = "driver=lime";
        std::string type = "fc32";
        this->soapy_source_0 = soapy::source::make(device, type, 1);
        this->soapy_source_0->set_sample_rate(0, sample_rate);
        this->soapy_source_0->set_bandwidth(0, sample_rate);
        this->soapy_source_0->set_frequency(0, center_freq);
        this->soapy_source_0->set_frequency_correction(0, 0);
        this->soapy_source_0->set_gain(0, rx_gain);
        this->soapy_source_0->set_min_output_buffer((int)((1u << m_sf)*sample_rate/bw*20));
    }
    //--------------------------------
   
    this->cran_lora_frame_detector_0 = cran::lora_frame_detector_single_sf::make(this->sample_rate,m_sf);
    
    this->cran_rrh_zmq_sink_0 = cran::rrh_zmq_sink::make(config.broker_front_rrh_addr, config.rrh_channel_indices,this->sample_rate);
    
    // Connections:
    if(is_usrp){
        this->tb->hier_block2::connect(this->uhd_usrp_source_0, 0, this->cran_lora_frame_detector_0, 0);
    }
    else{
        this->tb->hier_block2::connect(this->soapy_source_0, 0, this->cran_lora_frame_detector_0, 0);
    }      
    this->tb->hier_block2::connect(this->cran_lora_frame_detector_0, 0, this->cran_rrh_zmq_sink_0, 0);

    // ------------- for debug ----------
    // this->tag_debug_0 = blocks::tag_debug::make(sizeof(gr_complex),"tag debug USRP out: ");
    // this->tb->hier_block2::connect(this->uhd_usrp_source_0, 0, this->tag_debug_0, 0);
    //----------------------------------- Set time ----------------------------------------
   
    if (use_GPS)
    {
        // open serial port
        LibSerial::SerialPort serial_port;
        // Open the hardware serial ports.
        //raspi4
        // std::string serial_port_name = "/dev/serial0";

        //raspi5
        std::string serial_port_name = "/dev/ttyAMA0";
        serial_port.Open(serial_port_name);
        debug_print("listening on port " << serial_port_name, m_identity, "rrh", YELLOW);
        // Set the baud rates.
        serial_port.SetBaudRate(BaudRate::BAUD_9600);
        std::string read_line;

        const ::uhd::time_spec_t last_pps_time = this->uhd_usrp_source_0->get_time_last_pps();
        bool valid_time = false;
        time_t timeSinceEpoch;
        // wait to have a PPS
        while (last_pps_time == this->uhd_usrp_source_0->get_time_last_pps())
        {
            sleep(0.1);
        }

        // look for current time in NMEA serial stream
        std::size_t found = std::string::npos;
        while (found == std::string::npos)
        {
            serial_port.ReadLine(read_line, '\n');
             
            found = read_line.find("$GPRMC,");
            if (found != std::string::npos)
            {
                //std::cout<<"char: "<<read_line<<"\n";
                // parse comma separated values
                std::vector<std::string> parsed_line;

                std::stringstream ss(read_line);

                while (ss.good())
                {
                    std::string substr;
                    getline(ss, substr, ',');
                    parsed_line.push_back(substr);
                }
                // std::cout<<" ------- Parsed NMEA msg---------\n";
                // for (int i = 0; i < parsed_line.size(); i++)
                // {
                //     std::cout<<"val: "<<parsed_line[i]<<std::endl;
                // }
                // std::cout<<" ----------------\n";

                std::string time = parsed_line[1];
                std::string date = parsed_line[9];
                valid_time = parsed_line[2].data()[0] == 'A';

                std::cout << "Got NMEA msg: time_valid:" << valid_time << ", current time: " << time << ", date: " << date << std::endl;
                // convert to epoch time
                struct tm t = {0}; // Initalize to all 0's
                t.tm_mday = stoi(date.substr(0, 2));
                t.tm_mon = stoi(date.substr(2, 2)) - 1;    // month start from jan=0
                t.tm_year = stoi(date.substr(4, 2)) + 100; // This is GPRMC_year+2000-1900
                t.tm_hour = stoi(time.substr(0, 2));
                t.tm_min = stoi(time.substr(2, 2));
                t.tm_sec = stoi(time.substr(4, 2));
                timeSinceEpoch = timegm(&t);
            }
        }

        if (!valid_time)
        {
            std::cerr << "no valid time recovered from NMEA string!" << std::endl;
        }
        this->uhd_usrp_source_0->set_time_next_pps(::uhd::time_spec_t(timeSinceEpoch + 1, 0));
        std::cout << "Set USRP absolute time next PPS to: "<< timeSinceEpoch + 1<< std::endl;

        // sleep to let the time be set properly
        sleep(2);
    }
    else //set time based on local clock
    {
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        
        uint64_t now_frac_sec_tmp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

        uint64_t now_full_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        double now_frac_sec = double(now_frac_sec_tmp-now_full_sec*1e6)/1e6;
        //this->uhd_usrp_source_0->set_time_now(::uhd::time_spec_t(now_full_sec, now_frac_sec));
        //std::cout<<"now : "<<now_full_sec<<" "<<now_frac_sec<<std::endl;

        //sleep(2);
    }
}

raspi_single_channel_rrh::~raspi_single_channel_rrh()
{
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
