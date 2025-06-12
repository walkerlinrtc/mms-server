#include "bitrate_monitor.h"
using namespace mms;

void BitrateMonitor::on_bytes_in(size_t bytes) {
    in_bytes_ += bytes;
}

void BitrateMonitor::on_bytes_out(size_t bytes) {
    out_bytes_ += bytes;
}

void BitrateMonitor::tick() {
    // double in_kbps = (in_bytes_ * 8.0) / 1000.0;
    // double out_kbps = (out_bytes_ * 8.0) / 1000.0;
    // 清零
    in_bytes_ = 0;
    out_bytes_ = 0;
}
