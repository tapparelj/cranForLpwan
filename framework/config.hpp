#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <string.h>
struct{
    //-------- IP address setup -------------
    std::string application_ip_addr = "127.0.0.1"; // IP address of the application server

    #pragma message("Replace the placeholder IP address of the baseband unit and remove this message")
    std::string bbu_ip_addr = "192.168.10.1"; // IP address of the BBU

    //-------- ZMQ address with port setup -------------
    // this choice of port number is arbitrary
    std::string application_address = "tcp://"+application_ip_addr+":70001";
    std::string buffer_pull_addr_start = "tcp://"+bbu_ip_addr+":60000"; //this port needs to be open in your firewall and +1 for each rrh
    std::string broker_front_rrh_addr = "tcp://"+bbu_ip_addr+":50001"; //this port needs to be open in your firewall

    //-------- Channels enabled--------------
    // for now, only a single channel can be open at a time due to the RRH code
    std::vector<int> rrh_channel_indices = {0}; //corresponds to the channels specified in gr-cran/include/gnuradio/cran/utilities.h
}config;
#endif
