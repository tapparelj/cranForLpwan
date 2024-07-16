#ifndef LORA_RX_BUFFER_HPP
#define LORA_RX_BUFFER_HPP
#include <string>
#include <iostream>

#include <zmq.hpp>
#include <gnuradio/top_block.h>
#include <queue>
#include "gnuradio/cran/zhelper.h"
#include "gnuradio/cran/utilities.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <uhd/types/time_spec.hpp>
#include <boost/filesystem.hpp>


#define TIMEOUT 10000
#define MAX_PORTS 10


class lora_rx_buffer
{
private:
  std::string m_pub_addr;
  std::string m_pull_addr;
  std::string m_broker_addr;
  std::string m_bufferDataSaveFolder;
  uint32_t m_msg_id;
  uint8_t m_topic;
  int m_waiting_for_worker; ///< number of worker we are waiting to continue streaming data
  bool keep_alive;

  bool m_is_streaming;
  bool m_need_new_socket;
  std::queue<std::string> m_pub_message_queue;

  //initialize zmq
  zmq::context_t m_context{1};
  // construct sockets
  zmq::socket_t* m_broker_socket;
  zmq::socket_t m_pub_socket{m_context, zmq::socket_type::pub};
  zmq::socket_t m_pull_socket{m_context, zmq::socket_type::pull};

  std::vector<uint32_t> m_frames_idx;
  zmq::message_t m_msg_buff;
  zmq::message_t m_msg_data_buff;

  int m_port_num = 60000;//pub port number

public:
    lora_rx_buffer(std::string broker_addr, std::string buffer_pull_addr, std::string bufferDataSaveFolder);
    ~lora_rx_buffer();
    void save_msg_to_file(const char *msg, uint64_t msg_size, bool new_file = false);
    int run();
};

#endif