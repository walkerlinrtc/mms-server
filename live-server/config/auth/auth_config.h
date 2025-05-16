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
    AuthConfig();
    virtual ~AuthConfig();
    int32_t check(std::shared_ptr<StreamSession> s);
    int32_t load(const YAML::Node & node);
protected:
    std::shared_ptr<Param> recursive_search_param_node(const YAML::Node & node, const std::string & param_name);
    int32_t parse_check_config(const YAML::Node & node);
    int32_t parse_method_param(const YAML::Node & node, std::shared_ptr<Param> param_holder, const std::string & method_param);
    int32_t parse_check_param(const YAML::Node & node, std::shared_ptr<CheckConfig> check, const std::string & method_param);
    std::shared_ptr<PlaceHolder> gen_holder(const YAML::Node & node, const std::string & seg, bool is_string_holder);
protected:
    std::vector<std::shared_ptr<CheckConfig>> checks_;
    std::map<std::string, std::shared_ptr<Param>> params_;
};
};