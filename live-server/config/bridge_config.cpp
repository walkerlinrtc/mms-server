#include <boost/algorithm/string.hpp>
#include "bridge_config.h"
#include "log/log.h"
using namespace mms;

BridgeConfig::BridgeConfig() {

}

BridgeConfig::~BridgeConfig() {

}

int32_t BridgeConfig::load(const YAML::Node & config) {
    auto no_players_timeout = config["no_players_timeout"];
    try {
        if (no_players_timeout.IsDefined()) {
            auto sno_players_timeout = no_players_timeout.as<std::string>();
            if (sno_players_timeout.ends_with("ms")) {
                auto s = sno_players_timeout.substr(0, sno_players_timeout.size() - 2);
                no_players_timeout_ms_ = std::atoi(s.c_str());
            } else if (sno_players_timeout.ends_with("s")) {
                auto s = sno_players_timeout.substr(0, sno_players_timeout.size() - 2);
                no_players_timeout_ms_ = std::atoi(s.c_str())*1000;
            }
        }
    } catch (std::exception & exp) {
        CORE_ERROR("read no_players_timeout failed");
        return -1;
    }

    YAML::Node rtmp = config["rtmp"];
    if (rtmp.IsDefined()) {
        YAML::Node rtmp_to_flv = rtmp["to_flv"];
        if (rtmp_to_flv.IsDefined()) {
            std::string v = rtmp_to_flv.as<std::string>();
            boost::algorithm::to_lower(v);
            rtmp_to_flv_ = v == "on";
        }

        YAML::Node rtmp_to_rtsp = rtmp["to_rtsp"];
        if (rtmp_to_rtsp.IsDefined()) {
            std::string v = rtmp_to_rtsp.as<std::string>();
            boost::algorithm::to_lower(v);
            rtmp_to_rtsp_ = v == "on";
        }

        YAML::Node rtmp_to_hls = rtmp["to_hls"];
        if (rtmp_to_hls.IsDefined()) {
            std::string v = rtmp_to_hls.as<std::string>();
            boost::algorithm::to_lower(v);
            rtmp_to_hls_ = v == "on";
        }

        YAML::Node rtmp_to_webrtc = rtmp["to_webrtc"];
        if (rtmp_to_webrtc.IsDefined()) {
            std::string v = rtmp_to_webrtc.as<std::string>();
            boost::algorithm::to_lower(v);
            rtmp_to_webrtc_ = v == "on";
        }
    }
    

    //================= flv =====================
    YAML::Node flv = config["flv"];
    if (flv.IsDefined()) {
        YAML::Node flv_to_rtmp = flv["to_rtmp"];
        if (flv_to_rtmp.IsDefined()) {
            std::string v = flv_to_rtmp.as<std::string>();
            boost::algorithm::to_lower(v);
            flv_to_rtmp_ = v == "on";
        }

        YAML::Node flv_to_rtsp = flv["to_rtsp"];
        if (flv_to_rtsp.IsDefined()) {
            std::string v = flv_to_rtsp.as<std::string>();
            boost::algorithm::to_lower(v);
            flv_to_rtsp_ = v == "on";
        }

        YAML::Node flv_to_hls = flv["to_hls"];
        if (flv_to_hls.IsDefined()) {
            std::string v = flv_to_hls.as<std::string>();
            boost::algorithm::to_lower(v);
            flv_to_hls_ = v == "on";
        }

        YAML::Node flv_to_webrtc = flv["to_webrtc"];
        if (flv_to_webrtc.IsDefined()) {
            std::string v = flv_to_webrtc.as<std::string>();
            boost::algorithm::to_lower(v);
            flv_to_webrtc_ = v == "on";
        }
    }
    
    //================= rtsp =====================
    YAML::Node rtsp = config["rtsp"];
    if (rtsp.IsDefined()) {
        YAML::Node rtsp_to_rtmp = rtsp["to_rtmp"];
        if (rtsp_to_rtmp.IsDefined()) {
            std::string v = rtsp_to_rtmp.as<std::string>();
            boost::algorithm::to_lower(v);
            rtsp_to_rtmp_ = v == "on";
        }

        YAML::Node rtsp_to_flv = rtsp["to_flv"];
        if (rtsp_to_flv.IsDefined()) {
            std::string v = rtsp_to_flv.as<std::string>();
            boost::algorithm::to_lower(v);
            rtsp_to_flv_ = v == "on";
        }

        YAML::Node rtsp_to_hls = rtsp["to_hls"];
        if (rtsp_to_hls.IsDefined()) {
            std::string v = rtsp_to_hls.as<std::string>();
            boost::algorithm::to_lower(v);
            rtsp_to_hls_ = v == "on";
        }

        YAML::Node rtsp_to_webrtc = rtsp["to_webrtc"];
        if (rtsp_to_webrtc.IsDefined()) {
            std::string v = rtsp_to_webrtc.as<std::string>();
            boost::algorithm::to_lower(v);
            rtsp_to_webrtc_ = v == "on";
        }
    }
    
    //================= webrtc =====================
    YAML::Node webrtc = config["webrtc"];
    if (webrtc.IsDefined()) {
        YAML::Node webrtc_to_rtmp = webrtc["to_rtmp"];
        if (webrtc_to_rtmp.IsDefined()) {
            std::string v = webrtc_to_rtmp.as<std::string>();
            boost::algorithm::to_lower(v);
            webrtc_to_rtmp_ = v == "on";
        }

        YAML::Node webrtc_to_flv = webrtc["to_flv"];
        if (webrtc_to_flv.IsDefined()) {
            std::string v = webrtc_to_flv.as<std::string>();
            boost::algorithm::to_lower(v);
            webrtc_to_flv_ = v == "on";
        }

        YAML::Node webrtc_to_hls = webrtc["to_hls"];
        if (webrtc_to_hls.IsDefined()) {
            std::string v = webrtc_to_hls.as<std::string>();
            boost::algorithm::to_lower(v);
            webrtc_to_hls_ = v == "on";
        }

        YAML::Node webrtc_to_rtsp = webrtc["to_rtsp"];
        if (webrtc_to_rtsp.IsDefined()) {
            std::string v = webrtc_to_rtsp.as<std::string>();
            boost::algorithm::to_lower(v);
            webrtc_to_rtsp_ = v == "on";
        }
    }
    
    return 0;
}