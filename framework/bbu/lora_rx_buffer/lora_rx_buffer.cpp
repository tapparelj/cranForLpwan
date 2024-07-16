#include "lora_rx_buffer.hpp"

lora_rx_buffer::lora_rx_buffer(std::string broker_addr, std::string buffer_pull_addr, std::string bufferDataSaveFolder)
{
  m_pub_addr = "tcp://127.0.0.1:*";
  m_pull_addr = buffer_pull_addr;
  m_broker_addr = broker_addr;
  m_waiting_for_worker = 0;
  m_bufferDataSaveFolder = bufferDataSaveFolder;

  // zmq binding
  m_broker_socket = new zmq::socket_t(m_context, zmq::socket_type::dealer);
  m_broker_socket->connect(m_broker_addr);
  m_broker_socket->set(zmq::sockopt::linger, 0);
  // m_pub_socket.set(zmq::sockopt::sndhwm,0);
  m_pub_socket.set(zmq::sockopt::linger, 0);
  m_pub_socket.bind(m_pub_addr);
  m_pub_socket.set(zmq::sockopt::linger, 0);
  m_pub_socket.set(zmq::sockopt::sndhwm, 0);
  m_pub_socket.set(zmq::sockopt::sndtimeo, 0);
  m_pub_addr = m_pub_socket.get(zmq::sockopt::last_endpoint);

  int cnt = 0;
  m_msg_id = 0;

  for (int i = 0; i< MAX_PORTS; i++)
  {
    try
    {
      debug_print("Try with "<<m_pull_addr,m_port_num,"rx_buffer",GREEN);
      m_pull_socket.bind(m_pull_addr);
      break;
    }
    catch (zmq::error_t err)
    {
      if (err.num() == EADDRINUSE) // try with next port
      {
        size_t pos = 0;
        std::string token;
        std::string tmp_addr;
        while ((pos = m_pull_addr.find(":")) != std::string::npos)
        {
          token = m_pull_addr.substr(0, pos);
          tmp_addr.append(token+":");
          std::cout << token << std::endl;
          m_pull_addr.erase(0, pos+1);
        }
        m_port_num = stoi(m_pull_addr)+1;
        m_pull_addr = tmp_addr + std::to_string(m_port_num);
      }
      else
      {
        break;
      }
    }
  }
  m_pull_socket.set(zmq::sockopt::linger, 0);
  m_pull_socket.set(zmq::sockopt::rcvhwm, 0); // no limit on the input queue size
  m_pull_addr = m_pull_socket.get(zmq::sockopt::last_endpoint);

  //create save folder
  if(m_bufferDataSaveFolder!="")
  {
    boost::filesystem::create_directories(m_bufferDataSaveFolder + "/"+std::to_string(m_port_num));
  }

  // send pull address to broker
  // frame info informs the broker that the buffer is ready
  gr::cran::Frame_info info;
  info.frame_state = gr::cran::READY;
  info.channel_id = 0;
  info.sf = 0;
  info.samp_rate = 0;
  info.time_full = 0;
  info.time_frac = 0;
  zmq::message_t message(sizeof(info));
  memcpy(message.data(), &info, sizeof(info));
  m_broker_socket->send(message, zmq::send_flags::sndmore);
  // s_sendmore(*m_broker_socket, "READY");
  s_send(*m_broker_socket, m_pull_addr);
  debug_print(m_port_num<<" Send pull addr: " << m_pull_addr<< ", pub addr: "<<m_pub_addr,m_port_num,"rx_buffer",GREEN);
  std::string msg = s_recv(*m_broker_socket);
  assert(msg.size() == 0);
  msg = s_recv(*m_broker_socket);
  assert(msg.compare("ACK") == 0);

  debug_print(" Buffer ready",m_port_num,"rx_buffer",GREEN);
  m_need_new_socket = false;
} 

lora_rx_buffer::~lora_rx_buffer() {}

void lora_rx_buffer::save_msg_to_file(const char *msg, uint64_t msg_size, bool new_file)
{
  if(m_bufferDataSaveFolder=="") return;

  if (new_file)
  {
    m_msg_id++;
  }
  std::string filename = m_bufferDataSaveFolder + "/"+ std::to_string(m_port_num)+"/"+std::to_string(m_msg_id) + ".bin";

  std::ofstream file;
  file.open(filename, std::ios::out | std::ios::binary | std::ios::app);

  debug_print("Saving message to file: "<<filename<<", size: "<<msg_size, m_port_num,"rx_buffer",GREEN);

  file.write(reinterpret_cast<const char *>(&msg_size), sizeof(msg_size));
  file.write(msg, msg_size);
  file.close();
}

int lora_rx_buffer::run()
{
  zmq::pollitem_t items[] = {
      {*m_broker_socket, 0, ZMQ_POLLIN, 0},
      {m_pull_socket, 0, ZMQ_POLLIN, 0}};
  int header_cnt = 0;
  while (1)
  {
    // poll both interfaces
    if (m_pub_message_queue.size())
      zmq::poll(items, 2, 0);
    else
      zmq::poll(items, 2, std::chrono::seconds(TIMEOUT));
    if ((items[1].revents & ZMQ_POLLIN)) // got smth from the RRH
    {
      bool has_topic = false;
      while (1) // read frame headers
      { 
        zmq::recv_result_t res = m_pull_socket.recv(m_msg_buff);       

        if (m_msg_buff.size() == 0) 
        {
          //only save if this is a delimiter. Not an empty frame to keep buffer alive
          if(has_topic)
          {
            save_msg_to_file(reinterpret_cast<const char *>(m_msg_buff.data()), m_msg_buff.size());
          }
          // debug_print(" Rec empty delimiter",m_port_num,"rx_buffer",GREEN);
          break;
        }
        else // read header
        {
          has_topic = true;
          gr::cran::Frame_info *header = (gr::cran::Frame_info *)m_msg_buff.data();
          header_cnt++;
          //m_topic = (*header & CHANNEL_MASK);
          m_topic = header->channel_id;
          // debug_print("Rec header 0x"<<std::hex<<(*header)<<std::dec);
          bool is_new_frame = (header->frame_state) == gr::cran::NEW_FRAME;
          if (is_new_frame) // we need a new worker
          {
            //save in a new file if there is a frame start and we are not currently streaming
            save_msg_to_file(reinterpret_cast<const char *>(m_msg_buff.data()), m_msg_buff.size(), is_new_frame && (header_cnt==1));
            if (m_need_new_socket) // if no socket
            {
              debug_print(" we need a socket to the broker!!",m_port_num,"rx_buffer",GREEN);
              m_broker_socket = new zmq::socket_t(m_context, zmq::socket_type::req);
              m_broker_socket->connect(m_broker_addr);
              m_broker_socket->set(zmq::sockopt::linger, 0);
              m_need_new_socket = false;
            }
            debug_print(" Requesting new worker for channel "<<int(header->channel_id)<<", sf "<<(int)header->sf<<", time "<<header->time_full<<" "<<header->time_frac,m_port_num,"rx_buffer",GREEN);

            for (int i = 0; i < m_pub_message_queue.size(); i++)
            {
              warning_print("Cleaned message"<<i<<" of size "<<m_pub_message_queue.back().size()<<" to start new work stream",m_port_num);

            
              m_pub_message_queue.pop();
            }
            // m_debugfile<<int(header->channel_id)<<","<<(int)header->sf<<","<<(int)header->time_full<<","<<header->time_frac<<std::endl;
            
            debug_print("[Time] Start of new work at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us",m_port_num,"rx_buffer",GREEN);
            m_broker_socket->send(m_msg_buff, zmq::send_flags::sndmore);
            
            s_send(*m_broker_socket, m_pub_addr);
            m_waiting_for_worker++;
          }
          else //Just need to stream samples
          {
            save_msg_to_file(reinterpret_cast<const char *>(m_msg_buff.data()), m_msg_buff.size());
          }  
        }
      }
      if (!has_topic) // received empty message to keep buffer alive
      { debug_print(" buffer stay alive",m_port_num,"rx_buffer",GREEN);
        continue;
      }
      // read time of first sample
      m_pull_socket.recv(m_msg_data_buff);
      save_msg_to_file(reinterpret_cast<const char *>(m_msg_data_buff.data()), m_msg_data_buff.size());


      ::uhd::time_spec_t first_sample_time;
      memcpy(&first_sample_time,m_msg_data_buff.data(),sizeof(::uhd::time_spec_t));
      // debug_print("First_sample time: "<<m_msg_data_buff.size()<<"/"<<sizeof(::uhd::time_spec_t)<<" "<< first_sample_time.get_full_secs()<<" "<<first_sample_time.get_frac_secs(),m_port_num,"rx_buffer",CYAN);

      zmq::recv_result_t msg_size = m_pull_socket.recv(m_msg_data_buff);
      assert (msg_size != -1);
      save_msg_to_file(reinterpret_cast<const char *>(m_msg_data_buff.data()), m_msg_data_buff.size());
      
      // prepare message payload with topic and time prepend
      std::string new_message;
      new_message.resize(sizeof(m_topic) + sizeof(::uhd::time_spec_t) + msg_size.value());
      
      memcpy((uint8_t *)new_message.data(), &m_topic, sizeof(m_topic));
      memcpy((uint8_t *)new_message.data() + sizeof(m_topic), &first_sample_time, sizeof(::uhd::time_spec_t));
      memcpy((uint8_t *)new_message.data() + sizeof(m_topic) + sizeof(::uhd::time_spec_t), m_msg_data_buff.data(), m_msg_data_buff.size());

      m_pub_message_queue.push(new_message);
    }
    if ((items[0].revents & ZMQ_POLLIN)) // got smth from the broker a worker is listening
    {
      std::string reply = s_recv(*m_broker_socket);
      debug_print(" Received " << reply,m_port_num,"rx_buffer",GREEN);
      if (reply.compare("SUBSCRIBED") == 0)
      {
        m_waiting_for_worker--;
        debug_print(" Worker listening, waiting for "<<m_waiting_for_worker<<" workers",m_port_num,"rx_buffer",GREEN);
      }
    }
    if (!(items[0].revents & ZMQ_POLLIN) && !(items[1].revents & ZMQ_POLLIN) && !m_pub_message_queue.size())
    {
      // TIMEOUT kill buffer
      debug_print(" Timeout, buffer terminating",m_port_num,"rx_buffer",GREEN);
      m_broker_socket->close();
      m_pub_socket.close();
      m_pull_socket.close();

      return 0;
    }
    //debug_print(RED<<" m_waiting_for_worker "<<m_waiting_for_worker<<"m_pub_message_queue.size() "<<m_pub_message_queue.size() <<RESET);
    if (!m_waiting_for_worker && m_pub_message_queue.size())
    {
      // debug_print("pub queue size: " << m_pub_message_queue.size());

      while (!m_pub_message_queue.empty())
      {
        // m_pub_socket.send(m_pub_message_queue.back(), zmq::send_flags::none);
        // if (m_pub_message_queue.size() > 1)
        //   s_sendmore(m_pub_socket, m_pub_message_queue.front());
        // else

        ::uhd::time_spec_t first_sample_time;
        memcpy(&first_sample_time, m_pub_message_queue.front().data() + sizeof(m_topic) , sizeof(::uhd::time_spec_t));
        debug_print("[Debug] published : "<<m_pub_message_queue.front().size()<<"B, with time: "<<first_sample_time.get_full_secs()<<" "<<first_sample_time.get_frac_secs() ,m_port_num,"rx_buffer",GREEN);
  
        auto res = s_send(m_pub_socket, m_pub_message_queue.front());

        m_pub_message_queue.pop();
      }
    }
  }
  return 0;
}

