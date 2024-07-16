#include "application.hpp"


bool isnot_alnum(char ch)
{
    return !isalnum(ch);
}

application::application(std::string pull_addr, std::string savefile) : m_pull_addr(pull_addr)
{
  m_savefile = savefile;

  si = new influxdb_cpp::server_info(flux_host, flux_port, flux_bucket, flux_usr, flux_passwd, "ns", flux_token);

}
application::~application()
{
  zmq_close(m_pull_socket);

  delete si;
}
int application::run()
{
  // initialize zmq
  debug_print("Started Application listening on " << m_pull_addr,"","Application",RESET);
  m_pull_socket.setsockopt(ZMQ_LINGER, 0);
  m_pull_socket.bind(m_pull_addr);

  zmq::pollitem_t items[] = {m_pull_socket, 0, ZMQ_POLLIN, 0};
  std::string input_buffer;
  int cnt = 0;
  if (m_savefile.compare("") != 0)
  {

    boost::filesystem::path dir(m_savefile);
    boost::filesystem::path containing_folder = dir.parent_path();

    if(!(boost::filesystem::exists(containing_folder))){
        std::cout<<containing_folder<<"Doesn't Exists"<<std::endl;

        if (boost::filesystem::create_directories(containing_folder))
            std::cout<<containing_folder << "....Successfully Created !" << std::endl;
    }
    result_file.open(m_savefile, std::ios::out | std::ios::trunc);
    debug_print("Saving results to: "<<m_savefile,"","Application",RESET);

    

  }

  while (1)
  {
    zmq::poll(items, 1, -1);
    if ((items[0].revents & ZMQ_POLLIN))
    {

      // input_buffer = s_recv(m_pull_socket);
      // crc_check = input_buffer.compare("INVALID");
      // debug_print("crc_check"<< crc_check);
      zmq::message_t msg;
      m_pull_socket.recv(msg);
      bool crc_check;
      memcpy(&crc_check, msg.data(), sizeof(crc_check));

      // SF
      m_pull_socket.recv(msg);
      uint8_t sf;
      memcpy(&sf, msg.data(), sizeof(sf));

      // code rate
      m_pull_socket.recv(msg);
      uint8_t cr;
      memcpy(&cr, msg.data(), sizeof(cr));

      // number of rrh involved
      m_pull_socket.recv(msg);
      uint8_t n_rrh_involved;
      memcpy(&n_rrh_involved, msg.data(), sizeof(n_rrh_involved));

      // deserialize rrh infos
      input_buffer = s_recv(m_pull_socket);

      pmt::pmt_t rrh_info_vect = pmt::deserialize_str(input_buffer);

      //received payload
      input_buffer = s_recv(m_pull_socket);
      // encode in base64
      std::string encoded_payload = macaron::Base64::Encode(input_buffer);

      // debug_print("Rx msg: "<<input_buffer);
      // replace all non alphanumeric characters  with dots in received message to avoid csv issues
      // causes no issue when we only send alphanumeric characters
      //std::replace_if(input_buffer.begin(), input_buffer.end(), isnot_alnum, '.');
      for(int i=0; i<n_rrh_involved;i++){  
        //decode RRH info
        pmt::pmt_t rrh_info_dict = pmt::vector_ref(rrh_info_vect,i);
        pmt::pmt_t err = pmt::string_to_symbol("error");

        std::string buffer_addr = pmt::symbol_to_string(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("buffer_addr"), err));
        float sig_pow = pmt::to_float(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("sig_pow"), err));
        float noise_pow = pmt::to_float(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("noise_pow"), err));
        float cfo = pmt::to_float(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("cfo"), err));
        uint64_t time_full = pmt::to_uint64(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("time_full"), err));
        double time_frac = pmt::to_double(pmt::dict_ref(rrh_info_dict, pmt::string_to_symbol("time_frac"), err));
        float snr = 10*std::log10(sig_pow/noise_pow);
        std::cout<<"Received: "<<encoded_payload << ","<<(int) sf<<","<<(int) cr<<","<<(int)crc_check<<","<<(int)n_rrh_involved<<","<<buffer_addr <<","<< sig_pow <<","<< noise_pow <<","<<snr<<","<< cfo <<","<< time_full <<","<<std::setprecision(9)<<std::fixed<<time_frac<<std::defaultfloat<<std::endl;

        // Logging to influxdb
        int ret = influxdb_cpp::builder()
        .meas(flux_meas)
        .tag("rrh_id", buffer_addr)       //// To-Do: maps the buffer addresses to the corresponding RRH id (grafana dashboard queries were constructed with id in [0, 3])
        .field("sf", sf)
        .field("cr", cr)
        .field("crc_check", (int) crc_check)  //// current dashboard is constructed with for crc_check of type int; To-Do, modify the dashboard to bool and update the code. 
        .field("n_rrh_involved", n_rrh_involved)
        .field("sig_pow", sig_pow)
        .field("noise_pow", noise_pow)
        .field("snr", snr)
        .field("cfo", cfo)
        .timestamp(time_full + time_frac*1e9)
        .post_http(*si);

        if(ret)
          std::cout << RED << "influxdb logging error with code " << ret << RESET << std::endl;

        //save to file if filename not empty
        if (m_savefile.compare("") != 0)
        {
          // int ms = std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::system_clock::now().time_since_epoch()).count();
          // result_file << input_buffer << "," << m_snr << "," << crc_check << "," << buffers_addr.size() << "," << buffers_addr[0] <<","<<ms<< std::endl;
          result_file << encoded_payload << ","<<(int) sf<<","<<(int) cr<<","<<(int)crc_check<<","<<(int)n_rrh_involved<<","<<buffer_addr <<","<< sig_pow <<","<< noise_pow <<","<<snr<<","<< cfo <<","<< time_full <<","<<std::setprecision(9)<<std::fixed<<time_frac<<std::defaultfloat<<std::endl;
        }
      }
      if (!(++cnt % 10))
        debug_print("Got " << cnt << " messages","","Application",RESET);

      // print string as hex values
      // std::ostringstream result;
      // result << std::setw(2) << std::setfill('0') << std::hex << std::uppercase;
      // std::copy(input_buffer.begin(), input_buffer.end(), std::ostream_iterator<unsigned int>(result, " "));
      // std::cout << input_buffer << ":" << result.str() << std::endl;
    }
  }
  return 0;
}

void application::setFluxHost(std::string host){
  flux_host = host;
}

void application::setFluxPort(uint16_t port){
  flux_port = port;
}

void application::setFluxBucket(std::string bucket){
  flux_bucket = bucket;
}

void application::setFluxMeas(std::string meas){
  flux_meas = meas;
}

void application::setFluxUser(std::string usr){
  flux_usr = usr;
}

void application::setFluxPasswd(std::string passwd){
  flux_passwd = passwd;
}

void application::setFluxToken(std::string token){
  flux_token = token;
}