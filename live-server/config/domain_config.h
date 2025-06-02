#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "json/json.h"

namespace mms {
class AppConfig;
class Config;
class DomainConfig;

class DomainConfig : public std::enable_shared_from_this<DomainConfig> {
public:
    DomainConfig(Config & top_conf);
    bool load_config(const std::string & file);
    Json::Value to_json();
    const Config & get_top_conf() const {
        return top_conf_;
    }

    std::unordered_set<std::string> get_apps();
    std::shared_ptr<AppConfig> get_app_conf(const std::string & app_name);
    std::shared_ptr<DomainConfig> get_publish_domain_config() {
        return publish_domain_config_;
    }

    const std::string & get_publish_domain_name() const;
private:
    Config & top_conf_;
    
    std::string type_;//域名类型，publish：推流域名，play：播放域名
    std::string domain_name_;
    std::string publish_domain_name_;//如果type是play，那么publish_domain_name不能是空
    std::string key_file_;
    std::string cert_file_;

    std::unordered_map<std::string, std::shared_ptr<AppConfig>> app_confs_;
    std::shared_ptr<DomainConfig> publish_domain_config_;
};
};