#include "system.h"
#include "base/thread/thread_worker.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/read.hpp>
#include <fstream>
#include <string>

using namespace mms;

System System::instance_;
System &System::get_instance() { return instance_; }

System::System() {}

System::~System() {}

void System::init(ThreadWorker *worker) {
    worker_ = worker;
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            timer_ = std::make_shared<boost::asio::steady_timer>(worker_->get_io_context());
            while (1) {
                timer_->expires_after(std::chrono::milliseconds(1000));
                co_await timer_->async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    break;
                }
                do_sample();
            }
            co_return;
        }, boost::asio::detached);
}

static bool read_cpu_stat(uint64_t &idle_time, uint64_t &total_time) {
    std::ifstream stat("/proc/stat");
    if (!stat.is_open()) return false;

    std::string line;
    std::getline(stat, line); // 只读取第一行
    std::istringstream iss(line);

    std::string cpu;
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    idle_time = idle + iowait;
    total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    return true;
}

void System::do_sample() {
    auto now = time(NULL);
    auto mem_info = get_mem_info();
    mem_usages_[now] = mem_info.usage_percent;
    if (mem_usages_.size() >= MAX_POINTS) {
        mem_usages_.erase(mem_usages_.begin());
    }
    curr_mem_usage_.store(mem_info.usage_percent);

    uint64_t idle_time;
    uint64_t total_time;
    if (read_cpu_stat(idle_time, total_time)) {
        uint64_t idle_delta = idle_time - idle_time_;
        uint64_t total_delta = total_time - total_time_;
        float usage = (1.0f - static_cast<float>(idle_delta) / total_delta) * 1000.0f;
        curr_cpu_usage_.store(usage);
        cpu_usages_[now] = usage;
        if (cpu_usages_.size() >= MAX_POINTS) {
            cpu_usages_.erase(cpu_usages_.begin());
        }
    }
}

void System::uninit() {
    if (timer_) {
        timer_->cancel();
    }
}

MemoryInfo System::get_mem_info() const {
    std::ifstream meminfo("/proc/meminfo");
    MemoryInfo info;

    if (!meminfo.is_open()) {
        return info;
    }

    std::string line;
    long buffers = 0, cached = 0;

    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0)
            sscanf(line.c_str(), "MemTotal: %ld kB", &info.total_kb);
        else if (line.find("MemFree:") == 0)
            sscanf(line.c_str(), "MemFree: %ld kB", &info.free_kb);
        else if (line.find("MemAvailable:") == 0)
            sscanf(line.c_str(), "MemAvailable: %ld kB", &info.available_kb);
        else if (line.find("Buffers:") == 0)
            sscanf(line.c_str(), "Buffers: %ld kB", &buffers);
        else if (line.find("Cached:") == 0)
            sscanf(line.c_str(), "Cached: %ld kB", &cached);

        // 提前退出
        if (info.total_kb && info.free_kb && info.available_kb && buffers && cached) break;
    }

    long used_kb = info.total_kb - info.free_kb - buffers - cached;
    info.used_kb = used_kb;
    if (info.total_kb > 0) {
        info.usage_percent = static_cast<float>(used_kb) / info.total_kb * 100.0f;
    }

    return info;
}

Json::Value System::to_json() {
    Json::Value v;
    v["curr_cpu_usage"] = curr_cpu_usage_.load();
    v["curr_mem_usage"] = curr_mem_usage_.load();
    Json::Value jcpu_usages;
    for (auto & p : cpu_usages_) {
        Json::Value c;
        c["t"] = p.first;
        c["usage"] = p.second;
        jcpu_usages.append(c);
    }
    v["cpu_usages"] = jcpu_usages;

    Json::Value jmem_usages;
    for (auto & p : mem_usages_) {
        Json::Value c;
        c["t"] = p.first;
        c["usage"] = p.second;
        jmem_usages.append(c);
    }
    v["mem_usages"] = jmem_usages;
    return v;
}