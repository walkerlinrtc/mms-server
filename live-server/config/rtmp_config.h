#pragma once
#include <string>
#include <vector>
#include <memory>

#include "yaml-cpp/yaml.h"

namespace mms {
class RtmpConfig {
public:
    RtmpConfig();
    virtual ~RtmpConfig();

    int32_t load(const YAML::Node & node);
    
    bool is_enabled() const {
        return enabled_;
    }

    uint16_t port() const {
        return port_;
    }
    
    uint32_t out_chunk_size() const {
        return out_chunk_size_;
    }

protected:
    bool enabled_ = false;
    uint16_t port_ = 1935;
    uint32_t out_chunk_size_ = 409600;
};


};