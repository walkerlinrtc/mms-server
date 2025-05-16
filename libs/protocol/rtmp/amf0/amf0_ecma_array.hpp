#pragma once
#include <unordered_map>
#include <optional>
#include "json/json.h"

#include "amf0_def.hpp"
#include "amf0_number.hpp"
#include "amf0_boolean.hpp"
#include "amf0_string.hpp"
#include "amf0_null.hpp"
#include "amf0_undefined.hpp"
#include "amf0_obj_end.hpp"

namespace mms {
class Amf0EcmaArray : public Amf0Data {
public:
    using value_type = std::unordered_map<std::string, Amf0Data*>;
    static const AMF0_MARKER_TYPE marker = ECMA_ARRAY_MARKER;

    Amf0EcmaArray() : Amf0Data(ECMA_ARRAY_MARKER) {}
    virtual ~Amf0EcmaArray() {
        for(auto & p : properties_) {
            delete p.second;
        }
        properties_.clear();
    }

    const std::unordered_map<std::string, Amf0Data*> & get_value() {
        return properties_;
    }
public:
    Json::Value to_json();

    template <typename T>
    std::optional<typename T::value_type> get_property(const std::string& key)
    {
        auto it = properties_.find(key);
        if (it == properties_.end() || it->second->get_type() != T::marker) {
            return std::optional<typename T::value_type>();
        }
        return ((T*)it->second)->get_value();
    }

    int32_t decode(const uint8_t *data, size_t len);

    int32_t encode(uint8_t *buf, size_t len) const;
    // float, double ...
    void set_item_value(const std::string & k, double v) {
        Amf0Number *d = new Amf0Number;
        d->set_value(v);
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        properties_[k] = d;
    }

    std::unordered_map<std::string, Amf0Data*> properties_;
};
};