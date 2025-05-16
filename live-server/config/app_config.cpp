/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-31 14:29:47
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\app_config.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <iostream>

#include "log/log.h"
#include "app_config.h"
#include "config/cdn/origin_pull_config.h"
#include "config/cdn/edge_push_config.h"
#include "auth/auth_config.h"
using namespace mms;
AppConfig::AppConfig(const std::string & domain_name, std::weak_ptr<DomainConfig> domain_cfg) : domain_name_(domain_name), domain_cfg_(domain_cfg) {
    
}

int32_t AppConfig::load_config(const YAML::Node & node)
{
    auto name_node = node["name"];
    if (!name_node.IsDefined()) {
        return -1;
    }
    app_name_ = name_node.as<std::string>();
    CORE_INFO("load app config, domain:{}, app:{}", domain_name_, app_name_);

    auto http_callbacks_node = node["http_callbacks"];
    if (http_callbacks_node.IsDefined()) {
        CORE_INFO("start load on_publish http callbacks config domain:{}, app:{}", domain_name_, app_name_);
        auto on_publish_node = http_callbacks_node["on_publish"];
        if (on_publish_node.IsDefined() && on_publish_node.IsSequence()) {
            for (size_t i = 0; i < on_publish_node.size(); i++) {
                auto callback_node = on_publish_node[i];
                HttpCallbackConfig callback_config;
                auto ret = callback_config.load_config(callback_node);
                if (0 != ret) {
                    CORE_ERROR("load on publish http callback config failed, domain:{}, app:{}, ret:{}", domain_name_, app_name_, ret);
                    return -2;
                }
                on_publish_.push_back(callback_config);
            }
        }

        auto on_unpublish_node = http_callbacks_node["on_unpublish"];
        if (on_unpublish_node.IsDefined()) {
            for (size_t i = 0; i < on_unpublish_node.size(); i++) {
                auto callback_node = on_unpublish_node[i];
                HttpCallbackConfig callback_config;
                auto ret = callback_config.load_config(callback_node);
                if (0 != ret) {
                    CORE_ERROR("load on publish http callback config failed, domain:{}, app:{}, ret:{}", domain_name_, app_name_, ret);
                    return -2;
                }
                on_unpublish_.push_back(callback_config);
            }
        }
    }

    auto origin_pull_node = node["origin_pull"];
    if (origin_pull_node.IsDefined()) {
        auto origin_pull_config = std::make_shared<OriginPullConfig>();
        auto ret = origin_pull_config->load(origin_pull_node);
        if (0 != ret) {
            CORE_ERROR("load source config failed, domain:{}, app:{}, ret:{}", domain_name_, app_name_, ret);
            return -3;
        }
        origin_pull_config_ = origin_pull_config;
    }

    auto test_push_to_count_node = node["test_push_to_count"];
    if (test_push_to_count_node.IsDefined()) {
        auto push_to_node = node["push_to"];
        int32_t test_push_to_count = test_push_to_count_node.as<int32_t>();
        if (push_to_node.IsDefined()) {
            for (size_t i = 0; i < push_to_node.size(); i++) {
                auto push_node = push_to_node[i];
                for (int j = 0; j < test_push_to_count; j++) {
                    auto push_config = std::make_shared<EdgePushConfig>();
                    push_config->set_test_index(j);
                    auto ret = push_config->load(push_node);
                    if (0 != ret) {
                        CORE_WARN("load push config failed, domain:{}, app:{}, ret:{}", domain_name_, app_name_, ret);
                        continue;
                    }
                    edge_push_configs_.push_back(push_config);
                }
            }
        }
    } else {
        auto push_to_node = node["edge_push"];
        if (push_to_node.IsDefined()) {
            for (size_t i = 0; i < push_to_node.size(); i++) {
                auto push_node = push_to_node[i];
                auto push_config = std::make_shared<EdgePushConfig>();
                auto ret = push_config->load(push_node);
                if (0 != ret) {
                    CORE_WARN("load push config failed, ret:{}", ret);
                    continue;
                }
                edge_push_configs_.push_back(push_config);
            }
        }
    }
    
    auto publish_auth_node = node["publish_auth_check"];
    if (publish_auth_node.IsDefined()) {
        auto publish_auth = std::make_shared<AuthConfig>();
        if (0 != publish_auth->load(publish_auth_node)) {
            CORE_ERROR("load publish auth failed");
            return -4;
        }
        on_publish_auth_ = publish_auth;
    }

    auto play_auth_node = node["play_auth_check"];
    if (play_auth_node.IsDefined()) {
        auto play_auth = std::make_shared<AuthConfig>();
        if (0 != play_auth->load(play_auth_node)) {
            spdlog::error("load play auth failed");
            return -4;
        }
        on_play_auth_ = play_auth;
    }

    auto hls = node["hls"];
    if (hls.IsDefined()) {
        if (0 != hls_config_.load(hls)) {
            CORE_ERROR("load hls config failed");
            return -5;
        }
    }

    auto bridge = node["bridge"];
    if (bridge.IsDefined()) {
        if (0 != bridge_config_.load(bridge)) {
            CORE_ERROR("load bridge config failed");
            return -6;
        }
    }

    auto record_types = node["record_types"];
    if (record_types.IsDefined() && record_types.IsSequence()) {
        for (auto it = record_types.begin(); it != record_types.end(); it++) {
            record_types_.insert(it->as<std::string>());
        }
    }

    auto fmp4_min_seg_dur_node = node["fmp4_min_seg_dur"];
    if (fmp4_min_seg_dur_node.IsDefined()) {
        fmp4_min_seg_dur_ = fmp4_min_seg_dur_node.as<int32_t>();
    }

    auto fmp4_max_seg_dur_node = node["fmp4_max_seg_dur"];
    if (fmp4_max_seg_dur_node.IsDefined()) {
        fmp4_max_seg_dur_ = fmp4_max_seg_dur_node.as<int32_t>();
    }

    auto max_fmp4_seg_bytes_node = node["max_fmp4_seg_bytes"];
    if (max_fmp4_seg_bytes_node.IsDefined()) {
        max_fmp4_seg_bytes_ = max_fmp4_seg_bytes_node.as<int64_t>();
    }

    auto stream_resume_timeout = node["stream_resume_timeout"];
    if (stream_resume_timeout.IsDefined() && stream_resume_timeout.IsScalar()) {
        stream_resume_timeout_ = stream_resume_timeout.as<uint32_t>();
    }

    return 0;
}