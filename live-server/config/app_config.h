/*
 * @Author: jbl19860422
 * @Date: 2022-12-06 23:27:18
 * @LastEditTime: 2023-07-02 11:22:25
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\config\app_config.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <optional>
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include "yaml-cpp/yaml.h"
#include "http_callback_config.h"
#include "hls_config.h"
#include "bridge_config.h"
#include "record/record_config.h"

namespace mms {
class DomainConfig;
class OriginPullConfig;
class EdgePushConfig;
class AuthConfig;

class AppConfig {
public:
    AppConfig(const std::string &domain_name, std::weak_ptr<DomainConfig> domain_config);
    int32_t load_config(const YAML::Node & node);
    const std::string & get_app_name() const {
        return app_name_;
    }
    
    std::shared_ptr<DomainConfig> get_domain_config() const {
        return domain_cfg_.lock();
    }

    const std::vector<HttpCallbackConfig> & get_on_publish_config() const {
        return on_publish_;
    }

    const std::vector<HttpCallbackConfig> & get_on_unpublish_config() const {
        return on_unpublish_;
    }

    std::shared_ptr<OriginPullConfig> origin_pull_config() {
        return origin_pull_config_;
    }

    std::vector<std::shared_ptr<EdgePushConfig>> edge_push_configs() {
        return edge_push_configs_;
    }

    std::shared_ptr<AuthConfig> get_publish_auth_config() {
        return on_publish_auth_;
    }

    std::shared_ptr<AuthConfig> get_play_auth_config() {
        return on_play_auth_;
    }

    const HlsConfig & hls_config() const {
        return hls_config_;
    }

    const BridgeConfig & bridge_config() const {
        return bridge_config_;
    }

    const RecordConfig & record_config() const {
        return record_config_;
    }

    inline int32_t get_fmp4_min_seg_dur() const {
        return fmp4_min_seg_dur_;
    }

    inline int32_t get_fmp4_max_seg_dur() const {
        return fmp4_max_seg_dur_;
    }

    inline int64_t get_max_fmp4_seg_bytes() const {
        return max_fmp4_seg_bytes_;
    }

    inline uint32_t get_stream_resume_timeout() const {
        return stream_resume_timeout_;
    }
private:
    std::string domain_name_;
    std::string app_name_;
    std::weak_ptr<DomainConfig> domain_cfg_;
    std::vector<HttpCallbackConfig> on_publish_;
    std::vector<HttpCallbackConfig> on_unpublish_;
    // cdn 拉流和转推配置
    std::shared_ptr<OriginPullConfig> origin_pull_config_;
    std::vector<std::shared_ptr<EdgePushConfig>> edge_push_configs_;
    std::shared_ptr<AuthConfig> on_publish_auth_;
    std::shared_ptr<AuthConfig> on_play_auth_;
    // hls切片配置
    HlsConfig hls_config_;
    // 转协议配置
    BridgeConfig bridge_config_;
    // 录制相关
    RecordConfig record_config_;
    // dash配置相关
    int32_t fmp4_min_seg_dur_ = 2000;//fmp4切片最小时长，默认2秒，最小不能小于1秒
    int32_t fmp4_max_seg_dur_ = 60000;//fmp4切片最大时长，默认1分钟
    int64_t max_fmp4_seg_bytes_ = 10*1024*1024;//最大mp4 10M
    // 延迟删除
    uint32_t stream_resume_timeout_ = 10;//10秒删除流
};
};