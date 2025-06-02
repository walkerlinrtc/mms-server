#include "http_config.h"
#include "log/log.h"
using namespace mms;

StaticFileServerConfig::StaticFileServerConfig() {

}

StaticFileServerConfig::~StaticFileServerConfig() {

}

int32_t StaticFileServerConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    CORE_INFO("static file server is {}", enabled_);

    YAML::Node path_map = config["path_map"];
    if (path_map.IsDefined() && path_map.IsMap()) {
        for (auto it = path_map.begin(); it != path_map.end(); it++) {
            path_map_[it->first.as<std::string>()] = it->second.as<std::string>();
        }
    }

    return 0;
}

const std::unordered_map<std::string, std::string> & StaticFileServerConfig::get_path_map() const {
    return path_map_;
}

HttpConfig::HttpConfig() {

}

HttpConfig::~HttpConfig() {

}

int32_t HttpConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    YAML::Node port = config["port"];
    if (port.IsDefined()) {
        port_ = port.as<uint16_t>();
    }

    YAML::Node static_file_server_config = config["static_file_server"];
    if (static_file_server_config.IsDefined()) {
        if (0 != static_file_server_config_.load(static_file_server_config)) {
            return -1;
        }
    }

    return 0;
}

const StaticFileServerConfig & HttpConfig::get_static_file_server_config() const {
    return static_file_server_config_;
}