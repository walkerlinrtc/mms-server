#include <boost/algorithm/string.hpp>
#include "hls_config.h"
#include "log/log.h"
using namespace mms;

HlsConfig::HlsConfig() {

}

HlsConfig::~HlsConfig() {

}

int32_t HlsConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    auto ts_min_seg_dur_node = config["ts_min_seg_dur"];
    if (ts_min_seg_dur_node.IsDefined()) {
        ts_min_seg_dur_ = ts_min_seg_dur_node.as<int32_t>();
    }

    auto ts_max_seg_dur_node = config["ts_max_seg_dur"];
    if (ts_max_seg_dur_node.IsDefined()) {
        ts_max_seg_dur_ = ts_max_seg_dur_node.as<int32_t>();
    }

    auto ts_max_size_node = config["ts_max_size"];
    if (ts_max_size_node.IsDefined()) {
        auto v = ts_max_size_node.as<std::string>();
        if (v.ends_with("k") || v.ends_with("K")) {
            ts_max_size_ = std::atoi(v.substr(0, v.size() - 1).c_str())*1024;
        } else if (v.ends_with("m") || v.ends_with("M")) {
            ts_max_size_ = std::atoi(v.substr(0, v.size() - 1).c_str())*1024*1024;
        }
    }

    auto min_ts_count_for_m3u8 = config["min_ts_count_for_m3u8"];
    if (min_ts_count_for_m3u8.IsDefined()) {
        min_ts_count_for_m3u8_ = min_ts_count_for_m3u8.as<uint32_t>();
    }

    return 0;
}