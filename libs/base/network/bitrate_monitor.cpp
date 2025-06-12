#include "bitrate_monitor.h"
using namespace mms;

void BitrateMonitor::on_bytes_in(size_t bytes) {
    in_bytes_ += bytes;
}

void BitrateMonitor::on_bytes_out(size_t bytes) {
    out_bytes_ += bytes;
}

Json::Value BitrateMonitor::to_json() {
    Json::Value v;
    v["in_kbps"] = in_kbps_.load();
    v["out_kbps"] = out_kbps_.load();
    return v;
}

void BitrateMonitor::tick() {
    in_kbps_ = (in_bytes_ * 8.0) / 1000.0;
    out_kbps_ = (out_bytes_ * 8.0) / 1000.0;
    // 清零
    in_bytes_ = 0;
    out_bytes_ = 0;
}
