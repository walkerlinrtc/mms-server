#pragma once
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include "domain_config.h"
#include "http_callback_config.h"
#include "certs/certs_manager.h"
#include "rtmp_config.h"
#include "http_config.h"
#include "rtsp_config.h"
#include "webrtc/webrtc_config.h"

namespace mms {
class Config {
public:
    Config();
    static std::shared_ptr<Config> get_instance();
    /*
    * @brief 热更新配置
    * @param[in] config_path 配置路径
    */
    bool reload_config(const std::string & config_path);
    bool load_config(const std::string & config_path);
    const std::string & get_log_level() const {
        return log_level_;
    }

    std::shared_ptr<DomainConfig> get_domain_config(const std::string & domain_name);
    std::unordered_set<std::string> get_domains();
    
    const RtmpConfig & get_rtmp_config() const {
        return rtmp_config_;
    }

    const RtmpConfig & get_rtmps_config() const {
        return rtmps_config_;
    }

    const HttpConfig & get_http_config() const {
        return http_config_;
    }

    const HttpConfig & get_https_config() const {
        return https_config_;
    }

    const HttpConfig & get_http_api_config() const {
        return http_api_config_;
    }

    const HttpConfig & get_https_api_config() const {
        return https_api_config_;
    }

    const RtspConfig & get_rtsp_config() const {
        return rtsp_config_;
    }

    const RtspConfig & get_rtsps_config() const {
        return rtsps_config_;
    }

    const WebrtcConfig & get_webrtc_config() const {
        return webrtc_config_;
    }

    const std::string get_cert_root() const {
        return cert_root_;
    }

    CertManager & get_cert_manager() {
        return cert_manager_;
    }

    const std::string & get_record_root_path() {
        return record_root_path_;
    }

    uint32_t get_socket_inactive_timeout_ms() {
        return socket_inactive_timeout_ms_;
    }
private:
    bool load_domain_config(const std::string & domain, const std::string & file);
private:
    std::string log_level_ = "info";
    RtmpConfig rtmp_config_;
    RtmpConfig rtmps_config_;
    HttpConfig http_config_;
    HttpConfig https_config_;
    RtspConfig rtsp_config_;
    RtspConfig rtsps_config_;
    HttpConfig http_api_config_;
    HttpConfig https_api_config_;
    WebrtcConfig webrtc_config_;

    // uint16_t webrtc_udp_port_ = 8878;
    // std::string webrtc_ip_ = "";
    // std::string webrtc_internal_bind_ip_ = "";
    std::string cert_root_ = "certs";
    uint32_t socket_inactive_timeout_ms_ = 10000;// 默认10秒超时

    std::string record_root_path_;
    std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<DomainConfig>> domains_config_;
private:
    CertManager cert_manager_;
    static std::atomic<std::shared_ptr<Config>> instance_;
};
};