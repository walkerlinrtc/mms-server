#pragma once
#include <atomic>
#include "socket_traffic_observer.h"
#include "json/json.h"
namespace mms {
class BitrateMonitor : public SocketTrafficObserver {
public:
    void on_bytes_in(size_t bytes) override;
    void on_bytes_out(size_t bytes) override;
    Json::Value to_json();
    void tick(); // 定时调用，比如每秒一次
private:
    std::atomic<size_t> in_bytes_ = 0;
    std::atomic<size_t> out_bytes_ = 0;
    std::atomic<double> in_kbps_ = 0.0;
    std::atomic<double> out_kbps_ = 0.0;
};
}