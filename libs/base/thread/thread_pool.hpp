#pragma once
#include <functional>
#include <thread>
#include <memory>
#include <atomic>

#include <boost/serialization/singleton.hpp> 

#include "thread_worker.hpp"
#define RAND_WORKER -1
namespace mms {
class ThreadPool {
public:
    ThreadPool();
    virtual ~ThreadPool();

    void start(int cpu_count);
    void stop();

    ThreadWorker* get_worker(int cpu_num);
    std::vector<ThreadWorker*> & get_all_workers();
private:
    std::atomic<bool> running_;
    std::atomic<uint64_t> using_worker_idx_;
    std::vector<ThreadWorker*> workers_;
};

typedef boost::serialization::singleton<ThreadPool> thread_pool_inst;
};