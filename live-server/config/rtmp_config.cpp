#include <boost/algorithm/string.hpp>
#include "rtmp_config.h"
#include "log/log.h"
using namespace mms;
RtmpConfig::RtmpConfig() {

}

RtmpConfig::~RtmpConfig() {

}

int32_t RtmpConfig::load(const YAML::Node & config) {
    YAML::Node enabled = config["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    YAML::Node port = config["port"];
    if (port.IsDefined()) {
        port_ = port.as<uint16_t>();
    }

    YAML::Node out_chunk_size = config["out_chunk_size"];
    if (out_chunk_size.IsDefined()) {
        out_chunk_size_ = out_chunk_size.as<uint32_t>();
    }
    return 0;
}