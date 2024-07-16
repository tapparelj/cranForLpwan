#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <string>
#include <pthread.h>
#include <iostream>
#include <sstream>      
#include <chrono>
#include <algorithm>
#include <fstream>
#include <boost/filesystem.hpp>
#include "Base64.h"
#include <pmt/pmt.h>


#include <zmq.hpp>
#include <queue>
#include "gnuradio/cran/zhelper.h"
#include "gnuradio/cran/utilities.h"

class application {

private:
    zmq::context_t context{1};
    zmq::socket_t m_pull_socket{context, zmq::socket_type::pull};
    std::string m_pull_addr;
    bool crc_check;
    float m_snr;
    std::string m_savefile;
    std::vector<std::string> buffers_addr;
    std::ofstream result_file;

public:
    application(std::string pull_addr,std::string savefile);
    ~application();
    int run();
};



#endif