#include <sys/prctl.h>

#include "thread_pool.hpp"
using namespace mms;

ThreadPool::ThreadPool() : running_(false), using_worker_idx_(0) {
    
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start(int cpu_count) {
    if (running_) {
        return;
    }
    running_ = true;
    workers_.reserve(cpu_count);
    for (int i = 0; i < cpu_count; i++) {
        ThreadWorker *w = new ThreadWorker();
        w->set_cpu_core(i);
        w->start();
        workers_.emplace_back(w);
    }
}

void ThreadPool::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    for (auto & w : workers_) {
        w->stop();
        delete w;
    }
}

ThreadWorker* ThreadPool::get_worker(int cpu_num) {
    if (cpu_num == -1) {
        uint32_t idx = using_worker_idx_%workers_.size();
        using_worker_idx_++;
        return workers_[idx];
    }
    return workers_[cpu_num];
}

std::vector<ThreadWorker*> & ThreadPool::get_all_workers() {
    return workers_;
} 