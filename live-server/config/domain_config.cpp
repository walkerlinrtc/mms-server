/*
 * @Author: jbl19860422
 * @Date: 2023-07-22 13:24:41
 * @LastEditTime: 2023-07-22 15:39:31
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\domain_config.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <iostream>

#include "domain_config.h"
#include "yaml-cpp/yaml.h"
#include "app_config.h"
#include "app/app_manager.h"
#include "app/app.h"
#include "app/publish_app.h"
#include "app/play_app.h"

#include "certs/certs_manager.h"

#include "spdlog/spdlog.h"
#include "config.h"
#include "log/log.h"

using namespace mms;

DomainConfig::DomainConfig(Config & top_conf) : top_conf_(top_conf) {

}

bool DomainConfig::load_config(const std::string & file) {
    YAML::Node config = YAML::LoadFile(file);
    auto type_node = config["type"];
    if (!type_node.IsDefined()) {
        CORE_ERROR("load domain file:{} failed, no type defined");
        return false;
    }
    type_ = type_node.as<std::string>();

    auto name_node = config["name"];
    if (!name_node.IsDefined()) {
        CORE_ERROR("load domain file:{} failed, no name defined");
        return false;
    }
    domain_name_ = name_node.as<std::string>();

    if (type_ != "play" && type_ != "publish") {
        CORE_ERROR("domain config type can only be play or publish");
        return false;
    }

    if (type_ == "play") {
        auto publish_domain_node = config["publish_domain"];
        if (!publish_domain_node.IsDefined()) {
            CORE_ERROR("load domain file:{} failed, no publish_domain defined");
            return false;
        }
        publish_domain_name_ = publish_domain_node.as<std::string>();
        publish_domain_config_ = top_conf_.get_domain_config(publish_domain_name_);
        if (!publish_domain_config_) {
            CORE_ERROR("load domain file:{} failed, could not find publish domain config");
            return false;
        }
    }

    auto key_file_node = config["key_file"];
    if (key_file_node.IsDefined()) {
        key_file_ = key_file_node.as<std::string>();
    }

    auto cert_file_node = config["cert_file"];
    if (cert_file_node.IsDefined()) {
        cert_file_ = cert_file_node.as<std::string>();
    }

    if (!key_file_.empty() && !cert_file_.empty()) {
        if (0 != top_conf_.get_cert_manager().add_cert(domain_name_, key_file_, cert_file_)) {
            CORE_ERROR("load cert file:{}, {} for {} failed", key_file_, cert_file_, domain_name_);
            return false;
        }
    }

    auto app_names = AppManager::get_instance().get_domain_apps_name(domain_name_);
    std::unordered_map<std::string, bool> app_exist_map;
    for (auto & app_name : app_names) {
        app_exist_map[app_name] = false;
    }

    auto apps = config["apps"];
    for (size_t i = 0; i < apps.size(); i++) {
        YAML::Node app_node = apps[i];
        std::shared_ptr<AppConfig> app_config = std::make_shared<AppConfig>(domain_name_, std::weak_ptr<DomainConfig>(shared_from_this()));
        if (0 != app_config->load_config(app_node)) {
            CORE_ERROR("load app config failed, domain:{}", domain_name_);
            return false;
        }

        auto app = AppManager::get_instance().get_app(domain_name_, app_config->get_app_name());
        if (!app) {
            if (type_ == "publish") {
                app = std::make_shared<PublishApp>(domain_name_, app_config->get_app_name());
            } else {
                app = std::make_shared<PlayApp>(domain_name_, app_config->get_app_name());
                // 如果是播放app，查找推流app，并添加绑定
                auto publish_app = AppManager::get_instance().get_app(publish_domain_name_, app_config->get_app_name());
                if (!publish_app || !publish_app->is_publish_app()) {
                    CORE_ERROR("could not find publish app for domain:{}, app:{}", publish_domain_name_, app_config->get_app_name());
                    return false;
                }
                auto play_app = std::static_pointer_cast<PlayApp>(app);
                play_app->set_publish_app(std::static_pointer_cast<PublishApp>(publish_app));
            }

            app->init();
            AppManager::get_instance().add_app(domain_name_, app_config->get_app_name(), app);
        }

        app->update_conf(app_config);
        CORE_INFO("load app config succeed, domain:{}, app:{}", domain_name_, app_config->get_app_name());
    }

    // 配置文件更新后，如果app被删了，需要移除已经不存在的app
    for (auto & p : app_exist_map) {
        if (!p.second) {
            AppManager::get_instance().remove_app(domain_name_, p.first);
        }
    }
    
    return true;
}

const std::string & DomainConfig::get_publish_domain_name() const {
    return publish_domain_name_;
}

std::shared_ptr<AppConfig> DomainConfig::get_app_conf(const std::string & app_name) {
    auto it = app_confs_.find(app_name);
    if (it == app_confs_.end()) {
        return nullptr;
    }
    return it->second;
}