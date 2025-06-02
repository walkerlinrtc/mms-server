#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "yaml-cpp/yaml.h"
#include "json/json.h"

namespace mms {
class PlaceHolder;
class Param;
class StreamSession;
class HttpCallbackConfig {
public:
    int32_t load_config(const YAML::Node & node);    
    const std::string & get_protocol() const {
        return protocol_;
    }

    const std::string & get_domain_name() const {
        return target_domain_;
    }

    uint16_t get_port() const {
        return port_;
    }

    std::string gen_url(std::shared_ptr<StreamSession> s) const;
    std::string gen_body(std::shared_ptr<StreamSession> s) const;
    std::unordered_map<std::string, std::string> gen_headers(std::shared_ptr<StreamSession> s) const;

    Json::Value to_json();
public:
    std::string unformat_url_;
    std::unordered_map<std::string, std::string> unformat_params_;

    std::vector<std::shared_ptr<PlaceHolder>> url_holders_;

    std::string unformat_body_;
    std::vector<std::shared_ptr<PlaceHolder>> body_holders_;

    std::unordered_map<std::string, std::string> unformat_headers_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<PlaceHolder>>> headers_holders_;
    
    std::string target_domain_;
    std::string protocol_;
    uint16_t port_;
};

};