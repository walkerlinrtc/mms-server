#include <iostream>
#include <filesystem>
#include <mutex> 
#include <shared_mutex>

#include "log/log.h"
#include "config.h"
#include "yaml-cpp/yaml.h"
#include "base/utils/utils.h"

#include "spdlog/spdlog.h"
#include "service/dns/dns_service.hpp"
#include "service/conn/http_conn_pools.h"
#include "app/app_manager.h"

using namespace mms;
std::atomic<std::shared_ptr<Config>> Config::instance_;

Config::Config() : cert_manager_(*this) {

}

std::shared_ptr<Config> Config::get_instance() {
    auto inst = instance_.load();
    if (!inst) [[unlikely]] {
        inst = std::make_shared<Config>();
        instance_.store(inst);
    }
    return inst;
}

bool Config::load_config(const std::string & config_path) {
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_path + "/mms.yaml");
    } catch (YAML::ParserException & ex) {
        return false;
    } catch (YAML::BadFile &ex) {
        return false;
    }

    YAML::Node log_level = config["log_level"];
    if (log_level.IsDefined()) {
        log_level_ = log_level.as<std::string>();
    }

    YAML::Node rtmp = config["rtmp"];
    if (rtmp.IsDefined()) {
        if (0 != rtmp_config_.load(rtmp)) {
            CORE_ERROR("load rtmp config failed");
            return false;
        }
    }

    YAML::Node rtmps = config["rtmps"];
    if (rtmps.IsDefined()) {
        if (0 != rtmps_config_.load(rtmps)) {
            CORE_ERROR("load rtmps config failed");
            return false;
        }
    }

    YAML::Node http = config["http"];
    if (http.IsDefined()) {
        if (0 != http_config_.load(http)) {
            CORE_ERROR("load http config failed");
            return false;
        }
    }

    YAML::Node https = config["https"];
    if (https.IsDefined()) {
        if (0 != https_config_.load(https)) {
            CORE_ERROR("load https config failed");
            return false;
        }
    }

    YAML::Node http_api = config["http_api"];
    if (http_api.IsDefined()) {
        if (0 != http_api_config_.load(http_api)) {
            CORE_ERROR("load http api config failed");
            return false;
        }
    }

    YAML::Node https_api = config["https_api"];
    if (https_api.IsDefined()) {
        if (0 != https_api_config_.load(https_api)) {
            CORE_ERROR("load https api config failed");
            return false;
        }
    }

    YAML::Node rtsp = config["rtsp"];
    if (rtsp.IsDefined()) {
        if (0 != rtsp_config_.load(rtsp)) {
            CORE_ERROR("load rtsp config failed");
            return false;
        }
    }

    YAML::Node rtsps = config["rtsps"];
    if (rtsps.IsDefined()) {
        if (0 != rtsps_config_.load(rtsps)) {
            CORE_ERROR("load rtsps config failed");
            return false;
        }
    }

    YAML::Node record_root_path = config["record_root_path"];
    if (record_root_path.IsDefined()) {
        record_root_path_ = record_root_path.as<std::string>();
    }

    YAML::Node webrtc = config["webrtc"];
    if (webrtc.IsDefined()) {
        if (0 != webrtc_config_.load(webrtc)) {
            CORE_ERROR("load webrtc config failed");
            return false;
        }
    }

    YAML::Node cert_root = config["cert_root"];
    if (cert_root.IsDefined()) {
        cert_root_ = cert_root.as<std::string>();
    }

    YAML::Node socket_timeout = config["socket_inactive_timeout"];
    if (socket_timeout.IsDefined() && socket_timeout.IsScalar()) {
        auto s = socket_timeout.as<std::string>();
        if (s.ends_with("s")) {
            socket_inactive_timeout_ms_ = std::atoi(s.c_str())*1000;
        } else if (s.ends_with("ms")) {
            socket_inactive_timeout_ms_ = std::atoi(s.c_str());
        } else {
            CORE_ERROR("wrong socket inactive timeout config");
        }
    }

    auto domains = AppManager::get_instance().get_domains();
    std::unordered_map<std::string, bool> domains_exist_map;
    for (auto & domain : domains) {
        domains_exist_map[domain] = false;
    }

    for (const auto & file_entry : std::filesystem::directory_iterator(config_path + "/publish")) {
        if (!file_entry.is_regular_file() || file_entry.path().extension() != ".yaml") {
            continue;
        }

        const std::string & name = file_entry.path().filename().string();
        size_t point_pos = name.find_last_of(".");
        std::string domain = name.substr(0, point_pos);
        CORE_INFO("find config of domain:{}", domain);
        if (!load_domain_config(domain, file_entry.path().string())) {
            CORE_ERROR("load config of domain:{} failed, file path:{}", domain, file_entry.path().string());
            continue;
        }
        domains_exist_map[domain] = true;
    }

    for (const auto & file_entry : std::filesystem::directory_iterator(config_path + "/play")) {
        if (!file_entry.is_regular_file() || file_entry.path().extension() != ".yaml") {
            continue;
        }

        const std::string & name = file_entry.path().filename().string();
        size_t point_pos = name.find_last_of(".");
        std::string domain = name.substr(0, point_pos);
        CORE_INFO("find config of domain:{}", domain);
        if (!load_domain_config(domain, file_entry.path().string())) {
            CORE_ERROR("load config of domain:{} failed, file path:{}", domain, file_entry.path().string());
            continue;
        }
        domains_exist_map[domain] = true;
    }

    for (auto & p : domains_exist_map) {
        if (!p.second) {//已经不存在的，删除
            AppManager::get_instance().remove_domain(p.first);
        }
    }

    return true;
}

bool Config::reload_config(const std::string & config_path) {
    std::shared_ptr<Config> new_config = std::make_shared<Config>();
    if (!new_config->load_config(config_path)) {
        return false;
    }

    instance_.store(new_config);
    return true;
}

bool Config::load_domain_config(const std::string & domain, const std::string & file) {
    auto config = std::make_shared<DomainConfig>(*this);
    auto ret =  config->load_config(file);
    if (!ret) {
        return false;
    }

    std::unique_lock<std::shared_mutex> lck(mutex_);
    domains_config_[domain] = config;
    return true;
}

std::unordered_set<std::string> Config::get_domains() {
    std::unordered_set<std::string> domains;
    std::unique_lock<std::shared_mutex> lck(mutex_);
    for (auto & p : domains_config_) {
        domains.insert(p.first);
    }
    return domains;
}

std::shared_ptr<DomainConfig> Config::get_domain_config(const std::string & domain_name) {
    std::shared_lock<std::shared_mutex> lck(mutex_);
    auto it = domains_config_.find(domain_name);
    if (it == domains_config_.end()) {
        return nullptr;
    }
    return it->second;
}