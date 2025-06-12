#pragma once
#include "socket_traffic_observer.h"
namespace mms {
class BitrateMonitor : public SocketTrafficObserver {
public:
    void on_bytes_in(size_t bytes) override;
    void on_bytes_out(size_t bytes) override;

    void tick(); // 定时调用，比如每秒一次
private:
    size_t in_bytes_ = 0;
    size_t out_bytes_ = 0;
};
}