#pragma once
#include <string>
#include <vector>
#include <memory>

#include "yaml-cpp/yaml.h"

namespace mms {
class PlaceHolder;
class Param;
class StreamSession;

class EdgePushConfig {
public:
    EdgePushConfig();
    virtual ~EdgePushConfig();

    bool is_enabled() const {
        return enabled_;
    }
    
    const std::string & get_protocol() const {
        return protocol_;
    }
    
    const std::string & get_target_domain() const {
        return target_domain_;
    }
    
    std::string gen_url(std::shared_ptr<StreamSession> s);
    int32_t load(const YAML::Node & node);
    void set_test_index(int index) {
        test_index_ = index;
    }
protected:
    bool enabled_ = false;
    std::string protocol_;
    std::string unformat_url_;
    std::string target_domain_;
    int test_index_ = -1;
    std::vector<std::shared_ptr<PlaceHolder>> url_holders_;
    std::map<std::string, std::shared_ptr<Param>> params_;
};


};