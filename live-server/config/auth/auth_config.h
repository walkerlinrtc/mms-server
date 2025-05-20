#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "yaml-cpp/yaml.h"

namespace mms {
class PlaceHolder;
class Param;
class StreamSession;
class CheckConfig;

class AuthConfig {
public:
    AuthConfig() = default;
    ~AuthConfig() = default;
    int32_t check(std::shared_ptr<StreamSession> s);
    int32_t load(const YAML::Node & node);
    bool is_enabled() const {
        return enabled_;
    }
protected:
    int32_t parse_check_config(const YAML::Node & node, const YAML::Node & parent_node);
protected:
    bool enabled_;
    std::vector<std::shared_ptr<CheckConfig>> checks_;
    std::map<std::string, std::shared_ptr<Param>> params_;
};
};