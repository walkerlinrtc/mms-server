#pragma once
#include <vector>
#include <string>
#include <unordered_set>
#include "yaml-cpp/yaml.h"
namespace mms {
class RecordConfig {
public:
    RecordConfig() = default;
    virtual ~RecordConfig() = default;
public:
    int32_t load_config(const YAML::Node & node);
    bool is_enabled() const {
        return enabled_;
    }

    const std::unordered_set<std::string> & get_types() const {
        return types_;
    }
protected:
    bool enabled_ = false;
    std::unordered_set<std::string> types_;
};
};