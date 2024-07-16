#ifndef BROKER_HPP
#define BROKER_HPP

#include <string>
#include <pthread.h>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <boost/bind/bind.hpp>
#include <uhd/types/time_spec.hpp>
#include <zmq.hpp>
#include <gnuradio/top_block.h>
#include <queue>
#include "gnuradio/cran/utilities.h"
#include "gnuradio/cran/zhelper.h"
#include "lora_rx_worker/lora_rx_worker.hpp"
#include "lora_rx_sync_worker/lora_rx_sync_worker.hpp"
#include "lora_rx_demod_worker/lora_rx_demod_worker.hpp"
#include "lora_rx_buffer/lora_rx_buffer.hpp"
#include "../config.hpp"

#define WORKER_KEEP_ALIVE_PERIOD 8000 // in ms
#define WORKER_MARGIN 1           // number of worker we want available in the queue
#define MAX_IPC_FILES 65535 //maximum number of concurrently open IPC files (one per combiner)
#define MAX_THREADS 100 //maximum number of threads spawned simulataneously
#define MAX_DELAY_TO_COMBINE 0.1 //max delay to try combining the frames received at different RRHs (in seconds)
#define MIN_NUMBER_OF_WORKERS 1 //number of workers spawned at the beginning

struct worker_arg_struct
{
    char *broker_addr_back;
    char *application_addr;
};
struct demod_worker_arg_struct
{
    std::string broker_addr_back;
    std::string combiner_input_addr;
    std::string application_addr;
};
struct buffer_arg_struct
{
    std::string broker_addr_front_buffer;
    std::string buffer_pull_addr;
    std::string bufferDataSaveFolder;
};
struct Local_work_info{
    uint16_t work_id;
    std::vector<std::string> worker_id; ///< all workers involved in this work
    std::vector<std::string> buffers_id; ///< all buffers involved in this work
    std::string demod_worker_addr; ///< the address of the worker responsible for the joint combining
    uint8_t channel_id;
    uint8_t sf;
    uint64_t time_full; ///< time of the arrival at the first RRH
    double time_frac;
    int sync_worker_cnt; ///< counts the number of synchronization workers related to this work
    // std::vector<pthread_t> threads; ///< all buffers involved in this work
    };

struct buffer_struct
{
    std::string id;
    std::string rrh_id;
    std::vector<std::vector<uint16_t>> workers_by_fc; ///< list of carrier frequency with each active worker fc x workers
};

class broker
{
private:
    char m_identity[10] = {}; //unique identifier
    std::string m_broker_addr_start;
    std::string m_broker_addr_front_rrh;
    std::string m_application_addr;
    std::string m_broker_addr_back = "tcp://127.0.0.1:*";
    std::string m_broker_addr_front_buffer = "tcp://127.0.0.1:*";
    std::string m_bufferDataSaveFolder;
    struct worker_arg_struct worker_args;
    struct demod_worker_arg_struct demod_worker_args[MAX_THREADS];
    struct buffer_arg_struct buffer_args;
    uint32_t m_thread_id_cnt = 0;
    std::queue<std::string> m_rrh_list;        ///< list of RRH waiting for a buffer
    std::vector<buffer_struct> m_buffer_list;  ///< active list of buffers
    gr::cran::Frame_info m_frame_info;         ///< info of the new frame of a buffer
    std::vector<Local_work_info> m_work_list; ///< active list of works. many workers can be involved in the same work
    std::string m_buffer_id;                   ///< ID of th buffer requesting a worker
    int m_sync_thread_spawned = 0;
    int m_demod_thread_spawned = 0;

    uint16_t m_last_work_id = 0; ///< id of the last work started
    uint32_t m_ipc_file_idx = 0;

    gr::cran::Work_info work_summary_to_forward;

    std::vector<std::vector<uint16_t>> m_empty_worker_list;

    // std::queue<std::string> m_worker_queue; ///< Queue of available workers replaced by pools

    std::queue<std::string> m_sync_worker_pool; ///< pool of available synchronization workers
    std::queue<std::string> m_demod_worker_pool; ///< pool of available demodulation workers

    void forward_job_to_worker(Local_work_info &work_info, gr::cran::Frame_info frame_info, std::string buffer_pub_addr, zmq::socket_t *backend);

    Local_work_info* should_combine(Local_work_info new_work); ///< return a pointer to the work that we should combine with or m_work_list.end() if no combining possible

public:
    broker(std::string broker_addr_start, std::string broker_addr_front_rrh, std::string application_addr, std::string bufferDataSaveFolder);
    ~broker();
    int run();
};

#endif