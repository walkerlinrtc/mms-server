#pragma once
#include <string>
#include <vector>
#include <memory>

#include "yaml-cpp/yaml.h"

namespace mms {
class RtspConfig {
public:
    RtspConfig();
    virtual ~RtspConfig();

    int32_t load(const YAML::Node & node);
    
    bool is_enabled() const {
        return enabled_;
    }

    uint16_t port() const {
        return port_;
    }
    
protected:
    bool enabled_ = false;
    uint16_t port_ = 1935;
};


};