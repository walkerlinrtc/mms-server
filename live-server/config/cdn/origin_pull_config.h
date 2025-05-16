#pragma once
#include <string>
#include <vector>
#include <memory>

#include "yaml-cpp/yaml.h"

namespace mms {
class PlaceHolder;
class Param;
class StreamSession;

class OriginPullConfig {
public:
    OriginPullConfig();
    virtual ~OriginPullConfig();

    const std::string & get_protocol() const {
        return protocol_;
    }
    
    const std::string & get_target_domain() const {
        return target_domain_;
    }

    int64_t no_players_timeout_ms() const {
        return no_players_timeout_ms_;
    }
    
    std::string gen_url(std::shared_ptr<StreamSession> s);
    int32_t load(const YAML::Node & node);
protected:
    std::string protocol_;
    std::string unformat_url_;
    std::string target_domain_;
    std::vector<std::shared_ptr<PlaceHolder>> url_holders_;
    std::map<std::string, std::shared_ptr<Param>> params_;
    int64_t no_players_timeout_ms_ = 10000;//多久没人播放算超时接收拉流
};
};