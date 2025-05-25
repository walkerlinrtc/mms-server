#include "record_config.h"

using namespace mms;

int32_t RecordConfig::load_config(const YAML::Node & node) {
    YAML::Node enabled = node["enabled"];
    if (enabled.IsDefined()) {
        enabled_ = enabled.as<bool>();
    }

    auto record_types = node["types"];
    if (record_types.IsDefined() && record_types.IsSequence()) {
        for (auto it = record_types.begin(); it != record_types.end(); it++) {
            types_.insert(it->as<std::string>());
        }
    }
    return 0;
}