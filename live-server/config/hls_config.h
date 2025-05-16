#pragma once
#include <stdint.h>
#include "yaml-cpp/yaml.h"
namespace mms {
class HlsConfig {
public:
    HlsConfig();
    virtual ~HlsConfig();

    int32_t load(const YAML::Node & node);

    bool is_enabled() const {
        return enabled_;
    }

    int32_t ts_min_seg_dur() const {
        return ts_min_seg_dur_;
    }

    int32_t ts_max_seg_dur() const {
        return ts_max_seg_dur_;
    }

    int64_t ts_max_size() const {
        return ts_max_size_;
    }

    uint32_t min_ts_count_for_m3u8() const {
        return min_ts_count_for_m3u8_;
    }
protected:
    bool enabled_ = false;
    int32_t ts_min_seg_dur_ = 2000;//ts切片最小时长，默认2秒，最小不能小于1秒
    int32_t ts_max_seg_dur_ = 60000;//ts切片最大时长，默认1分钟
    int64_t ts_max_size_ = 10*1024*1024;//最大ts 10M
    uint32_t min_ts_count_for_m3u8_ = 3;//默认至少3个ts切片，才输出m3u8
};
};