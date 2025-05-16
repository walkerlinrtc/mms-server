#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "yaml-cpp/yaml.h"

namespace mms {
class StaticFileServerConfig {
public:
    StaticFileServerConfig();
    virtual ~StaticFileServerConfig();
    int32_t load(const YAML::Node & node);
    bool is_enabled() const {
        return enabled_;
    }
    
    const std::unordered_map<std::string, std::string> & get_path_map() const;
protected:
    bool enabled_ = false;
    std::unordered_map<std::string, std::string> path_map_;
};
class HttpConfig {
public:
    HttpConfig();
    virtual ~HttpConfig();

    int32_t load(const YAML::Node & node);
    
    bool is_enabled() const {
        return enabled_;
    }

    uint16_t port() const {
        return port_;
    }

    const StaticFileServerConfig & get_static_file_server_config() const;
protected:
    bool enabled_ = false;
    uint16_t port_ = 80;
    StaticFileServerConfig static_file_server_config_;
};


};