#include "system.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
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
        [this, self]() -> boost::asio::awaitable<void> {
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

void System::do_sample() {

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

CpuInfo System::get_cpu_usage() const {
    uint64_t idle1 = 0, total1 = 0;
    uint64_t idle2 = 0, total2 = 0;

    if (!read_cpu_stat(idle1, total1)) return CpuInfo{-1.0f};

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 采样间隔

    if (!read_cpu_stat(idle2, total2)) return CpuInfo{-1.0f};

    uint64_t idle_delta = idle2 - idle1;
    uint64_t total_delta = total2 - total1;

    float usage = 0.0f;
    if (total_delta > 0) {
        usage = (1.0f - static_cast<float>(idle_delta) / total_delta) * 100.0f;
    }

    return CpuInfo{usage};
}