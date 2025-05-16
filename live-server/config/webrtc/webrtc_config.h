#pragma once
#include <stdint.h>
#include <string>

#include "yaml-cpp/yaml.h"

namespace mms {
class WebrtcConfig {
public:
    WebrtcConfig();
    virtual ~WebrtcConfig();
    int32_t load(const YAML::Node & config);

    bool is_enabled() const {
        return enabled_;
    }
    uint16_t get_udp_port() const;
    const std::string & get_ip() const;
    const std::string & get_internal_ip() const;
protected:
    bool enabled_ = false;
    uint16_t udp_port_ = 8878;
    std::string ip_;
    std::string internal_ip_;//内网ip（像阿里云eip只能看到内外网ip不一致，需要配置）
};
};
