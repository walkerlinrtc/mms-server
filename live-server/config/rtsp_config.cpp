#include <boost/algorithm/string.hpp>
#include "rtsp_config.h"
#include "log/log.h"
using namespace mms;
RtspConfig::RtspConfig() {

}

RtspConfig::~RtspConfig() {

}

int32_t RtspConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    YAML::Node port = config["port"];
    if (port.IsDefined()) {
        port_ = port.as<uint16_t>();
    }

    return 0;
}