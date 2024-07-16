#include "broker.hpp"

broker::broker(std::string broker_addr_start, std::string broker_addr_front_rrh, std::string application_addr, std::string bufferDataSaveFolder) : 
  m_broker_addr_start(broker_addr_start),
  m_broker_addr_front_rrh(broker_addr_front_rrh),
  m_application_addr(application_addr),
  m_bufferDataSaveFolder(bufferDataSaveFolder)
{
  srandom(time(NULL));
  sprintf(m_identity, "%04X-%04X", within(0x10000), within(0x10000));
}

broker::~broker() {}

void *spawn_worker(void *arg)
{
  debug_print("Spawning new worker", "", "broker", GREEN);
  struct worker_arg_struct *args = (struct worker_arg_struct *)arg;
  // debug_print("worker bind " << broker_addr_back );
  lora_rx_worker *new_worker = new lora_rx_worker(args->broker_addr_back, args->application_addr);
  new_worker->tb->run();
  // print("Press Enter to quit: ";
  // std::cin.ignore();
  // new_worker->tb->stop();
  // new_worker->tb->wait();
  delete new_worker;
  debug_print("One worker terminated", "", "broker", GREEN);

  // return 0;
  pthread_exit(NULL);
}

void *spawn_sync_worker(void *arg)
{
  debug_print("Spawning new sync worker", "", "broker", GREEN);
  std::string *args = (std::string *)arg;
  // debug_print("worker bind " << broker_addr_back );
  lora_rx_sync_worker *new_llr_worker = new lora_rx_sync_worker(args->c_str());
  new_llr_worker->tb->run();
  // print("Press Enter to quit: ";
  // std::cin.ignore();
  // new_worker->tb->stop();
  // new_llr_worker->tb->wait();
  delete new_llr_worker;
  debug_print("One sync worker terminated", "", "broker", GREEN);

  // return 0;
  pthread_exit(NULL);
}

void *spawn_demod_worker(void *arg)
{
  debug_print("Spawning new worker", "", "broker", GREEN);
  struct demod_worker_arg_struct *args = (struct demod_worker_arg_struct *)arg;
  debug_print("demod worker bind input addr:" << args->combiner_input_addr, "", "broker", GREEN);
  lora_rx_demod_worker *new_demod_worker = new lora_rx_demod_worker(args->broker_addr_back, args->combiner_input_addr, args->application_addr);
  new_demod_worker->tb->run();
  // sleep(1);
  debug_print("One demod worker terminated!", "", "broker", GREEN);
  // new_llr_worker->tb->stop();

  // new_llr_worker->tb->wait();
  delete new_demod_worker;

  // return 0;
  pthread_exit(NULL);
}

void *spawn_buffer(void *arg)
{
  struct buffer_arg_struct *args = (struct buffer_arg_struct *)arg;
  lora_rx_buffer *lora_rx_buffer = new ::lora_rx_buffer(args->broker_addr_front_buffer, args->buffer_pull_addr, args->bufferDataSaveFolder);
  lora_rx_buffer->run();
  // debug_print("Press Enter to quit: ";
  // std::cin.ignore();
  // new_worker->tb->stop();
  delete lora_rx_buffer;
  debug_print("One buffer terminated", "", "broker", GREEN);

  return 0;
  pthread_exit(NULL);
}
Local_work_info *broker::should_combine(Local_work_info new_work)
{
  // Combined based on time of arrival
  // search for closest already existing work
  debug_print("new work id: " << new_work.work_id << " " << new_work.time_full << " " << new_work.time_frac, "", "broker", GREEN);
  uhd::time_spec_t diff;
  auto new_frame_time = uhd::time_spec_t(new_work.time_full, new_work.time_frac);
  for (auto it = m_work_list.begin(); it != m_work_list.end(); it++)
  {
    // get time diff
    diff = new_frame_time - uhd::time_spec_t(it->time_full, it->time_frac);
    debug_print("time diff:" << diff.get_real_secs(), "", "broker", GREEN);

    if (diff.get_real_secs() > 30)
    {
      debug_print("Something is very wrong with the time!", "", "broker", RED);
    }
    if (abs(diff.get_real_secs()) <= MAX_DELAY_TO_COMBINE)
    {
      return &*it;
    }
  }
  // TODO might change the loop to smth more elegant

  return &*m_work_list.end();
}

void broker::forward_job_to_worker(Local_work_info &work_info, gr::cran::Frame_info frame_info, std::string buffer_pub_addr, zmq::socket_t *backend)
{
  std::string worker_id = m_sync_worker_pool.front();
  work_info.sync_worker_cnt++;

  work_info.worker_id.push_back(worker_id);

  work_summary_to_forward.work_id = work_info.work_id;
  work_summary_to_forward.channel_id = work_info.channel_id;
  if (work_info.demod_worker_addr.size() > 64)
  {
    error_print("The IPC address is too big (more than 64 bytes)!", "broker");
  }
  strcpy(work_summary_to_forward.demod_worker_addr, work_info.demod_worker_addr.c_str());
  work_summary_to_forward.sf = work_info.sf;
  work_summary_to_forward.samp_rate = frame_info.samp_rate;
  work_summary_to_forward.bandwidth = frame_info.bandwidth;
  work_summary_to_forward.time_full = frame_info.time_full;
  work_summary_to_forward.time_frac = frame_info.time_frac;

  debug_print("rerouting job to worker: work id: "<<work_info.work_id<<", sync_worker_cnt: "<<work_info.sync_worker_cnt <<", worker ID: "<< string_to_hex(worker_id) << ", addr: " << work_info.demod_worker_addr << ", size: " << sizeof(work_summary_to_forward), m_identity, "broker", CYAN); // GREEN

  m_sync_worker_pool.pop();
  // Register this worker
  // uint16_t channel;
  // memcpy(&channel, frame_info->channel_id);
  // uint16_t fc_id = gr::cran::get_fc_id_from_channel(channel >> 1); // last bit is not the channel, just the type of message (new_frame or streaming)

  auto it = std::find_if(m_buffer_list.begin(), m_buffer_list.end(), boost::bind(&buffer_struct::id, boost::placeholders::_1) == work_info.buffers_id.back());
  if(it == m_buffer_list.end()){
    error_print("buffer not found!", m_identity);
  }
  it->workers_by_fc[frame_info.channel_id].push_back(work_info.work_id);
  //******************************************************
  debug_print("Buffer: " << string_to_hex(it->id) << ", new worker #" << it->workers_by_fc[work_info.channel_id].size() << " for channel " << (int)work_info.channel_id << " time " << frame_info.time_full, m_identity, "broker", CYAN);
  debug_print("work summary: " << work_summary_to_forward.work_id << ", channel_id " << (int)work_summary_to_forward.channel_id << ", demod_worker_addr " << work_summary_to_forward.demod_worker_addr << ", sf " << (int)work_summary_to_forward.sf << ", time_full " << work_summary_to_forward.time_full << ", time_frac " << work_summary_to_forward.time_frac << ", samp_rate " << work_summary_to_forward.samp_rate<< ", bandwidth " << work_summary_to_forward.bandwidth << ", buffer_pub_addr " << buffer_pub_addr, m_identity, "broker", CYAN);
  s_sendmore(*backend, worker_id);
  s_sendmore(*backend, "");
  s_sendmore(*backend, work_info.buffers_id.back());
  s_sendmore(*backend, "");
  zmq::message_t message(sizeof(work_summary_to_forward));
  memcpy(message.data(), &work_summary_to_forward, sizeof(work_summary_to_forward));
  backend->send(message, zmq::send_flags::sndmore);

  // s_sendmore(*backend, frame_info);
  s_send(*backend, buffer_pub_addr);
}

int broker::run()
{
  using namespace std;

  bool wait_sync_worker_spawn = false;
  bool continue_worker_attr = false;

  // initialize the zmq context with a single IO thread
  zmq::context_t context{1};

  // construct socket and bind to interfaces
  zmq::socket_t frontend_rrh{context, zmq::socket_type::router};
  zmq::socket_t frontend_buffer{context, zmq::socket_type::router};
  zmq::socket_t backend{context, zmq::socket_type::router};

  frontend_rrh.bind(m_broker_addr_front_rrh);
  frontend_buffer.bind(m_broker_addr_front_buffer);
  m_broker_addr_front_buffer = frontend_buffer.get(zmq::sockopt::last_endpoint);

  backend.bind(m_broker_addr_back);
  m_broker_addr_back = backend.get(zmq::sockopt::last_endpoint);
  debug_print("------------ Broker Interfaces ------------", m_identity, "broker", GREEN);
  debug_print("Buffer frontend: " << m_broker_addr_front_buffer, m_identity, "broker", GREEN);
  debug_print("RRH frontend: " << m_broker_addr_front_rrh, m_identity, "broker", GREEN);
  debug_print("Backend: " << m_broker_addr_back, m_identity, "broker", GREEN);
  debug_print("-------------------------------------------", m_identity, "broker", GREEN);

  // setup IPC directory
  if (!std::filesystem::exists("/tmp/cran"))
  {
    if (!std::filesystem::create_directory("/tmp/cran"))
    {
      error_print("failed to create folder for IPC!", m_identity);
    }
  }

  zmq::pollitem_t items[] = {
      {backend, 0, ZMQ_POLLIN, 0},
      {frontend_rrh, 0, ZMQ_POLLIN, 0},
      {frontend_buffer, 0, ZMQ_POLLIN, 0}};

  debug_print(GREEN << "Broker ready" << RESET, m_identity, "broker", GREEN);
  // init workers
  // simple worker args
  worker_args.broker_addr_back = (char *)m_broker_addr_back.c_str();
  worker_args.application_addr = (char *)m_application_addr.c_str();

  // joint processing worker args
  // sync_worker_args.broker_addr_back = m_broker_addr_back;

  //  init buffer params
  buffer_args.broker_addr_front_buffer = m_broker_addr_front_buffer;
  buffer_args.buffer_pull_addr = m_broker_addr_start;
  buffer_args.bufferDataSaveFolder = m_bufferDataSaveFolder;
  std::vector<uint16_t> empty{};
  m_empty_worker_list.resize(gr::cran::get_max_channels(), empty);

  // create minimum numbers of workers
  for (int i = 0; i < MIN_NUMBER_OF_WORKERS; i++)
  {
    //------------------- create a new demod worker -------------------
    pthread_t new_demod_worker;
    demod_worker_args[m_thread_id_cnt].combiner_input_addr = "ipc:///tmp/cran/" + std::to_string((m_ipc_file_idx++) % MAX_IPC_FILES);
    demod_worker_args[m_thread_id_cnt].broker_addr_back = m_broker_addr_back;
    demod_worker_args[m_thread_id_cnt].application_addr = m_application_addr;
    pthread_create(&new_demod_worker, NULL, spawn_demod_worker, (void *)&demod_worker_args[m_thread_id_cnt]);
    char demodnamebuffer[16];
    sprintf(demodnamebuffer, "demod_work_ %d", m_demod_thread_spawned++);
    pthread_setname_np(new_demod_worker, demodnamebuffer);
    assert(pthread_detach(new_demod_worker) == 0);
    debug_print("Spawning a new demod worker listening on " << demod_worker_args[m_thread_id_cnt].combiner_input_addr, m_identity, "broker", CYAN);
    m_demod_worker_pool.push(demod_worker_args[m_thread_id_cnt].combiner_input_addr);

    //------------------- create a new sync worker -------------------
    pthread_t new_sync_worker;
    pthread_create(&new_sync_worker, NULL, spawn_sync_worker, (void *)&m_broker_addr_back);
    char syncnamebuffer[16];
    sprintf(syncnamebuffer, "sync_work_%d", m_sync_thread_spawned++);
    pthread_setname_np(new_sync_worker, syncnamebuffer);

    assert(pthread_detach(new_sync_worker) == 0);
    debug_print("Spawning a new sync worker ", m_identity, "broker", CYAN);
    usleep(100*1000);//delay to simultaneous spawning issues
  }

  while (1)
  {
    //  Poll
    // if (m_worker_queue.size())
    zmq::poll(items, 3, std::chrono::seconds(WORKER_KEEP_ALIVE_PERIOD));
    // else
    //   zmq::poll(items, 1, -1);

    if (items[1].revents & ZMQ_POLLIN) // RRH request
    {
      debug_print("------ RRH Frontend -------", m_identity, "broker", GREEN);
      std::string rrh_id = s_recv(frontend_rrh);
      std::string buffer_request = s_recv(frontend_rrh);
      // verify if request for a new buffer
      assert(buffer_request.compare("NEW_BUFFER") == 0);
      m_rrh_list.push(rrh_id);
      debug_print("Creating new buffer for " << string_to_hex(rrh_id), m_identity, "broker", GREEN);

      pthread_t new_buffer;
      pthread_create(&new_buffer, NULL, spawn_buffer, static_cast<void *>(&buffer_args));
      assert(pthread_detach(new_buffer) == 0);
    }

    if (!wait_sync_worker_spawn && (items[2].revents & ZMQ_POLLIN))
    {
      debug_print("------ Buffer Frontend -------", m_identity, "broker", GREEN);
      if (!continue_worker_attr)
      {
        //  Now get next buffer request, route to LRU worker
        //  Client request is [address][empty][request]
        m_buffer_id = s_recv(frontend_buffer);
        // debug_print("New request from " << string_to_hex(buffer_addr));

        // request is the new frame info
        zmq::message_t m_frame_info_msg;
        auto r = frontend_buffer.recv(m_frame_info_msg);
        memcpy(&m_frame_info, m_frame_info_msg.data(), sizeof(m_frame_info));
        if (m_frame_info.frame_state == gr::cran::READY) // new buffer ready for a RRH
        {
          debug_print("Buffer " << string_to_hex(m_buffer_id) << " attributed to RRH " << string_to_hex(m_rrh_list.front()), m_identity, "broker", GREEN);
          buffer_struct new_buffer = {m_buffer_id, m_rrh_list.front(), m_empty_worker_list};

          m_buffer_list.push_back(new_buffer);

          s_sendmore(frontend_buffer, m_buffer_id);
          s_sendmore(frontend_buffer, "");
          s_send(frontend_buffer, "ACK");

          // answer to RRH with buffer address
          std::string buffer_pull_addr = s_recv(frontend_buffer);
          s_sendmore(frontend_rrh, m_rrh_list.front());
          s_sendmore(frontend_rrh, "");
          s_send(frontend_rrh, buffer_pull_addr);
          m_rrh_list.pop();
        }
        else // Buffer ask for a worker
        {
          // create new work
          Local_work_info new_work;
          new_work.work_id = m_last_work_id;
          new_work.buffers_id.push_back(m_buffer_id);
          new_work.channel_id = m_frame_info.channel_id;
          new_work.sf = m_frame_info.sf;
          new_work.time_full = m_frame_info.time_full;
          new_work.time_frac = m_frame_info.time_frac;

          // check if this should combine with an existing work
          auto prev_work = should_combine(new_work);
          if (prev_work != &*m_work_list.end()) // combine with existing work
          {
            debug_print("New work should be combined with work ID " << prev_work->work_id, m_identity, "broker", GREEN);

            prev_work->buffers_id.push_back(m_buffer_id);
            prev_work->sync_worker_cnt++;
          }
          else // this is a brand new work
          {
            //get start time of this new work
            // debug_print("[Time] new work starts at: "<<std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<" us",m_identity,"broker",GREEN);

            // check if a demod worker is available
            if (m_demod_worker_pool.size() == 0)
            { // we need a new demodulation worker
              pthread_t new_demod_worker;
              demod_worker_args[m_thread_id_cnt].combiner_input_addr = "ipc:///tmp/cran/" + std::to_string((m_ipc_file_idx++) % MAX_IPC_FILES);
              demod_worker_args[m_thread_id_cnt].broker_addr_back = m_broker_addr_back;
              demod_worker_args[m_thread_id_cnt].application_addr = m_application_addr;
              pthread_create(&new_demod_worker, NULL, spawn_demod_worker, (void *)&demod_worker_args[m_thread_id_cnt]);
              char demodnamebuffer[16];
              sprintf(demodnamebuffer, "demod_work_ %d", m_demod_thread_spawned++);
              pthread_setname_np(new_demod_worker, demodnamebuffer);
              assert(pthread_detach(new_demod_worker) == 0);
              debug_print("Spawning a new demod worker listening on " << demod_worker_args[m_thread_id_cnt].combiner_input_addr, m_identity, "broker", CYAN);
              new_work.demod_worker_addr = demod_worker_args[m_thread_id_cnt].combiner_input_addr;
              m_thread_id_cnt = (m_thread_id_cnt + 1) % MAX_THREADS;
            }
            else
            { // Reuse an already available worker
              new_work.demod_worker_addr = m_demod_worker_pool.front();
              m_demod_worker_pool.pop();
            }
            // add new work to the list
            new_work.sync_worker_cnt = 0;
            m_work_list.push_back(new_work);
            m_last_work_id++;
          }
          // we need a sync worker
          if (m_sync_worker_pool.size() == 0) // we should spawn a sync worker
          {
            continue_worker_attr = true;
            pthread_t new_sync_worker;

            // create a worker for synchronization
            pthread_create(&new_sync_worker, NULL, spawn_sync_worker, (void *)&m_broker_addr_back);
            char syncnamebuffer[16];
            

            sprintf(syncnamebuffer, "sync_work_%d", m_sync_thread_spawned++);
            pthread_setname_np(new_sync_worker, syncnamebuffer);

            assert(pthread_detach(new_sync_worker) == 0);
            debug_print("Spawning a new sync worker ", m_identity, "broker", CYAN);

            wait_sync_worker_spawn = true;
            continue;
          }
          else
          { // we can reuse a sync worker
            debug_print("One sync worker already available for new work", m_identity, "broker", CYAN);
            forward_job_to_worker(m_work_list.back(), m_frame_info, s_recv(frontend_buffer), &backend);
          }
        }
      }
      else if (m_sync_worker_pool.size() != 0) // finish sync worker attribution initiated previously
      {
        // std::string buffer_pub_addr = s_recv(frontend_buffer);
        forward_job_to_worker(m_work_list.back(), m_frame_info, s_recv(frontend_buffer), &backend); 
        continue_worker_attr = false;
      }
    }
    //  Handle worker activity on backend
    if (items[0].revents & ZMQ_POLLIN)
    {
      debug_print("----- Backend------", m_identity, "broker", GREEN);

      std::string worker_id = s_recv(backend);
      // debug_print("----- worker_id------ "<< string_to_hex(worker_id) );

      //  Second frame is empty
      std::string empty = s_recv(backend);

      if (empty.size() != 0)
      {
        error_print("It's supposed to be empty!! instead of : " << empty, m_identity);
      }; // assert

      //  Third frame is READY, DEMOD_DONE, SYNC_DONE or a client reply address
      std::string client_addr = s_recv(backend);
      // debug_print("----- client_addr------ "<< (client_addr) );

      if (client_addr.compare("READY") == 0) // new sync worker ready
      {
        m_sync_worker_pool.push(worker_id);
        debug_print("New sync worker ready: " << string_to_hex(worker_id), m_identity, "broker", CYAN);
        auto ready_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                
        debug_print("# of sync workers ready: " << m_sync_worker_pool.size(), m_identity, "broker", CYAN);
        wait_sync_worker_spawn = false;
      }
      else if (client_addr.compare("DEMOD_DONE") == 0 || client_addr.compare("SYNC_DONE") == 0) // received end of job notification from sync worker or demod worker
      {
        zmq::message_t msg_buff;
        auto r = backend.recv(msg_buff);

        uint16_t *work_id = (uint16_t *)msg_buff.data();

        // get work info based on work ID
        auto work_info = std::find_if(m_work_list.begin(), m_work_list.end(), boost::bind(&Local_work_info::work_id, boost::placeholders::_1) == *work_id);

        if (work_info == m_work_list.end())
        {
          error_print("[broker.cc] Work ID " << (int)*work_id << " doesn't match any undergoing work!", m_identity);
        }
        else
        {
          // check if all related works are done
          if (client_addr.compare("SYNC_DONE") == 0)
          {
            work_info->sync_worker_cnt--;
            debug_print("Sync worker_cnt for work " << (int)work_info->work_id << " changed to: " << work_info->sync_worker_cnt << ".", m_identity, "broker", GREEN);
          }
          else if (client_addr.compare("DEMOD_DONE") == 0)
          {
            debug_print("demod worker " << work_info->demod_worker_addr << " returned and is available again", m_identity, "broker", GREEN);
            m_demod_worker_pool.push(work_info->demod_worker_addr);
          }

          if (work_info->sync_worker_cnt <= 0 || client_addr.compare("DEMOD_DONE") == 0)
          { // We can terminate all workers and buffer streams related to this work
                           
            // terminate threads related to this work ID
            for (size_t i = 0; i < work_info->worker_id.size(); i++)
            {
              debug_print("send terminate to 0x" << string_to_hex(work_info->worker_id[i])<<" for work "<<(int)work_info->work_id, m_identity, "broker", GREEN);
              s_sendmore(backend, work_info->worker_id[i]);
              s_sendmore(backend, "");
              s_sendmore(backend, "DONE");
              zmq::message_t message(sizeof(uint16_t));
              memcpy(message.data(), &work_info->work_id, sizeof(uint16_t));
              backend.send(message, zmq::send_flags::none);
            }

            for (size_t i = 0; i < work_info->buffers_id.size(); i++)
            {
              std::string buffer_id = work_info->buffers_id[i];
              uint16_t channel_id = work_info->channel_id;
              // remove worker from active list
              auto it_buff = std::find_if(m_buffer_list.begin(), m_buffer_list.end(), boost::bind(&buffer_struct::id, boost::placeholders::_1) == buffer_id);

              auto worker_pos = std::find(it_buff->workers_by_fc[channel_id].begin(), it_buff->workers_by_fc[channel_id].end(), *work_id);
              if (worker_pos != it_buff->workers_by_fc[channel_id].end()) // == myVector.end() means the element was not found
                it_buff->workers_by_fc[channel_id].erase(worker_pos);
              else
                error_print("worker ID unkown!", m_identity);

              debug_print("Worker done for Buffer: " << string_to_hex(it_buff->id) << ", fc_id " << channel_id << ", work ID: " << *work_id << ", # of remaining workers : " << it_buff->workers_by_fc[channel_id].size(), m_identity, "broker", GREEN);
              // If no more active worker for this channel, tell the RRH to stop streaming
              if (it_buff->workers_by_fc[channel_id].size() == 0)
              {
                debug_print("RRH " << string_to_hex(it_buff->rrh_id) << " can stop streaming channel id " << channel_id, m_identity, "broker", GREEN);
                s_sendmore(frontend_rrh, it_buff->rrh_id);
                s_sendmore(frontend_rrh, "");
                // char fc_id_char[2];
                // memcpy(fc_id_char,&channel_id,2);
                // zmq::message_t message(sizeof(gr::cran::Frame_info));
                // memcpy(message.data(), frame_info, sizeof(gr::cran::Frame_info));
                // frontend_rrh.send(message, zmq::send_flags::none);
                zmq::message_t message(sizeof(uint16_t));
                memcpy(message.data(), &channel_id, sizeof(uint16_t));
                frontend_rrh.send(message, zmq::send_flags::none);
                // s_send(frontend_rrh, std::string(fc_id_char,2));
              }
            }

            // };
            // get decoding status id success, consider the worker as available again
            // std::string message = s_recv(backend);
            // if (message.compare("SUCCESS") == 0) // for now we don't reuse workers
            // {
            //   // m_worker_queue.push(worker_id);
            //   // debug_print("New worker: " << string_to_hex(worker_id));
            //   // debug_print("Workers ready: " << m_worker_queue.size());
            // }
            // remove work from the list
            m_work_list.erase(work_info);
          }
        }
      }
      else
      { //  worker reply, send rest back to frontend and ack worker
        std::string empty = s_recv(backend);
        assert(empty.size() == 0);

        // assert(empty.size() == 0);
        // forward to buffer frontend
        std::string reply = s_recv(backend);
        debug_print("forwarding " << reply << " to " << string_to_hex(client_addr), m_identity, "broker", GREEN);
        s_sendmore(frontend_buffer, client_addr);
        s_sendmore(frontend_buffer, "");
        s_send(frontend_buffer, reply);

        // acknlowledge backend
        // s_sendmore(backend, worker_id);
        // s_sendmore(backend, "");
        // s_send(backend, "ACK");
      }
    }
    else if (!(items[2].revents & ZMQ_POLLIN)) // we should keep some worker alive
    {
      int to_keep_alive = std::min((int)m_sync_worker_pool.size(), WORKER_MARGIN);
      for (int i = 0; i < to_keep_alive; i++)
      {
        std::string worker_addr = m_sync_worker_pool.front(); // m_worker_queue [0];
        debug_print("keeping worker: " << string_to_hex(worker_addr) << " alive", m_identity, "broker", GREEN);
        m_sync_worker_pool.pop();
        s_sendmore(backend, worker_addr);
        s_sendmore(backend, "");
        s_send(backend, "ACK");
      }
      // clear queue
      std::queue<std::string>().swap(m_sync_worker_pool);
    }
  }
  return 0;
}