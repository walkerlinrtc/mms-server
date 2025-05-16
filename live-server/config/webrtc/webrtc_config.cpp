#include <boost/algorithm/string.hpp>
#include "webrtc_config.h"
using namespace mms;

WebrtcConfig::WebrtcConfig() {

}

WebrtcConfig::~WebrtcConfig() {

}

int32_t WebrtcConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    auto udp_port = config["udp_port"];
    if (udp_port.IsDefined() && udp_port.IsScalar()) {
        udp_port_ = udp_port.as<uint16_t>();
    }

    auto ip = config["ip"];
    if (ip.IsDefined() && ip.IsScalar()) {
        ip_ = udp_port.as<std::string>();
    }

    auto internal_ip = config["internal_ip"];
    if (internal_ip.IsDefined() && internal_ip.IsScalar()) {
        internal_ip_ = internal_ip.as<std::string>();
    }
    return 0;
}

uint16_t WebrtcConfig::get_udp_port() const {
    return udp_port_;
}

const std::string & WebrtcConfig::get_ip() const {
    return ip_;
}

const std::string & WebrtcConfig::get_internal_ip() const {
    return internal_ip_;
}