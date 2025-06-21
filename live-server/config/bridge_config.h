#pragma once
#include "yaml-cpp/yaml.h"
namespace mms {
class BridgeConfig {
public:
    BridgeConfig();
    virtual ~BridgeConfig();

    int32_t load(const YAML::Node & config);

    bool rtmp_to_flv() const {
        return rtmp_to_flv_;
    }

    bool rtmp_to_rtsp() const {
        return rtmp_to_rtsp_;
    }

    bool rtmp_to_hls() const {
        return rtmp_to_hls_;
    }

    bool rtmp_to_webrtc() const {
        return rtmp_to_webrtc_;
    }

    bool rtmp_to_m4s() const {
        return rtmp_to_m4s_;
    }

    bool flv_to_rtmp() const {
        return flv_to_rtmp_;
    }

    bool flv_to_rtsp() const {
        return flv_to_rtsp_;
    }

    bool flv_to_hls() const {
        return flv_to_hls_;
    }

    bool flv_to_webrtc() const {
        return flv_to_webrtc_;
    }

    bool rtsp_to_rtmp() const {
        return rtsp_to_rtmp_;
    }

    bool rtsp_to_flv() const {
        return rtsp_to_flv_;
    }

    bool rtsp_to_hls() const {
        return rtsp_to_hls_;
    }

    bool rtsp_to_webrtc() const {
        return rtsp_to_webrtc_;
    }

    bool webrtc_to_rtmp() const {
        return webrtc_to_rtmp_;
    }

    bool webrtc_to_flv() const {
        return webrtc_to_flv_;
    }

    bool webrtc_to_hls() const {
        return webrtc_to_hls_;
    }

    bool webrtc_to_rtsp() const {
        return webrtc_to_rtsp_;
    }

    inline int64_t no_players_timeout_ms() const {
        return no_players_timeout_ms_;
    }
protected:
    // 超时配置
    int64_t no_players_timeout_ms_ = 10000;// 默认10秒
    bool rtmp_to_flv_ = false;
    bool rtmp_to_rtsp_ = false;
    bool rtmp_to_hls_ = false;
    bool rtmp_to_webrtc_ = false;
    bool rtmp_to_m4s_ = false;

    bool flv_to_rtmp_ = false;
    bool flv_to_rtsp_ = false;
    bool flv_to_hls_ = false;
    bool flv_to_webrtc_ = false;

    bool rtsp_to_rtmp_ = false;
    bool rtsp_to_flv_ = false;
    bool rtsp_to_hls_ = false;
    bool rtsp_to_webrtc_ = false;

    bool webrtc_to_rtmp_ = false;
    bool webrtc_to_flv_ = false;
    bool webrtc_to_hls_ = false;
    bool webrtc_to_rtsp_ = false;
};
};