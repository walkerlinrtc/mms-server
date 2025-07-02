#pragma once
#include <memory>
#include <queue>
#include <boost/asio/steady_timer.hpp>
#include "json/json.h"

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

struct CpuInfo {
    float usage_percent = 0.0f; // CPU 使用率百分比
};

class System {
public:
    static System & get_instance();
    void init(ThreadWorker *worker);
    void uninit();
    void do_sample();
    MemoryInfo get_mem_info() const;
    CpuInfo get_cpu_usage() const; // 新增
    
    virtual ~System();
private:
    System();
    System(const System &) = delete;            // 禁止拷贝
    System & operator=(const System &) = delete; // 禁止赋值
private:
    std::shared_ptr<boost::asio::steady_timer> timer_;
    static System instance_;
    ThreadWorker *worker_;

    std::deque<float> usage_history_; // 滚动窗口
    size_t max_points_;
};
};