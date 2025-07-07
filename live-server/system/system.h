#pragma once
#include <memory>
#include <queue>
#include <map>
#include <atomic>
#include <mutex>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include "json/json.h"
#include "base/wait_group.h"
#include "base/thread/thread_worker.hpp"

namespace mms {
class ThreadWorker;
struct MemoryInfo {
    long total_kb = 0;     // 总内存
    long free_kb = 0;      // 空闲内存
    long available_kb = 0; // 可用内存
    long used_kb = 0;           // 已使用内存
    float usage_percent = 0.0f; // 使用率百分比
    Json::Value to_json() {
        Json::Value root;
        root["total_kb"] = static_cast<Json::Int64>(total_kb);
        root["free_kb"] = static_cast<Json::Int64>(free_kb);
        root["available_kb"] = static_cast<Json::Int64>(available_kb);
        root["used_kb"] = static_cast<Json::Int64>(used_kb);
        root["usage_percent"] = usage_percent;
        return root;
    }
};

constexpr size_t MAX_POINTS = 5*60;
class System {
public:
    static System & get_instance();
    void init(ThreadWorker *worker);
    void uninit();
    void do_sample();
    MemoryInfo get_mem_info() const;
    virtual ~System();
    template <typename R> 
    boost::asio::awaitable<R> sync_exec(const std::function<R()> & exec_func) {
        WaitGroup wg(worker_);
        std::shared_ptr<R> result = std::make_shared<R>();
        wg.add(1);
        boost::asio::co_spawn(worker_->get_io_context(), [this, &wg, &exec_func, result]()->boost::asio::awaitable<void> {
            *result = exec_func();
            co_return;
        }, [this, &wg](std::exception_ptr exp) {
            (void)exp;
            wg.done();
        });

        co_await wg.wait();
        co_return *result;
    }

    Json::Value to_json();
private:
    System();
    System(const System &) = delete;            // 禁止拷贝
    System & operator=(const System &) = delete; // 禁止赋值
private:
    std::shared_ptr<boost::asio::steady_timer> timer_;
    static System instance_;
    ThreadWorker *worker_;

    uint64_t idle_time_ = 0;
    uint64_t total_time_ = 0;
    std::atomic<float> curr_cpu_usage_;
    std::atomic<float> curr_mem_usage_;
    std::map<int64_t, float> cpu_usages_;
    std::map<int64_t, float> mem_usages_;
};
};