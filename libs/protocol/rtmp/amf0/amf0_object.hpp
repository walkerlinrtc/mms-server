#pragma once
#include <optional>
#include <unordered_map>

#include "json/json.h"

#include "amf0_def.hpp"
#include "amf0_string.hpp"
#include "amf0_number.hpp"
#include "amf0_boolean.hpp"

namespace mms {
class Amf0Object : public Amf0Data {
public:
    using value_type = std::unordered_map<std::string, Amf0Data*>;
    static const AMF0_MARKER_TYPE marker = OBJECT_MARKER;

    Amf0Object()
        : Amf0Data(OBJECT_MARKER)
    {
    }
    virtual ~Amf0Object()
    {
        for (auto& p : properties_) {
            delete p.second;
        }
        properties_.clear();
    }

    template <typename T>
    std::optional<typename T::value_type> get_property(const std::string& key)
    {
        auto it = properties_.find(key);
        if (it == properties_.end() || it->second->get_type() != T::marker) {
            return std::optional<typename T::value_type>();
        }
        return ((T*)it->second)->get_value();
    }

    int32_t get_property_count() const {
        return properties_.size();
    }

    const std::unordered_map<std::string, Amf0Data*>& get_value()
    {
        return properties_;
    }

    void set_value(std::unordered_map<std::string, Amf0Data*>& v) {
        properties_ = v;
    }

    void set_item_value(const std::string & k, int v) {
        Amf0Number *d = new Amf0Number;
        d->set_value((double)v);
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        properties_[k] = d;
    }

    void set_item_value(const std::string & k, bool v) {
        Amf0Boolean *d = new Amf0Boolean;
        d->set_value(v);
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        properties_[k] = d;
    }

    void set_item_value(const std::string & k, const std::string &v) {
        Amf0String *d = new Amf0String;
        d->set_value(v);
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        properties_[k] = d;
    }

    void set_item_value(const std::string & k, const char *v) {
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        Amf0String *d = new Amf0String;
        d->set_value(v);
        properties_[k] = d;
    }

    void set_item_value(const std::string & k, Amf0Object *v) {
        auto it = properties_.find(k);
        if (it != properties_.end()) {
            delete it->second;
        }
        properties_[k] = v;
    }
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

    // template<typename T>
    // void set_item_value(const std::string & k, T v);

    int32_t decode(const uint8_t* data, size_t len);
    int32_t encode(uint8_t *buf, size_t len) const;

    size_t size() const {
        size_t s = 0;
        s = 1;// marker + count
        for (auto &p : properties_) {
            s += 2 + p.first.size();// key_len + key_str
            s += p.second->size();
        }
        s += 3;//obj end
        return s;
    }

    Json::Value to_json();
public:
    std::unordered_map<std::string, Amf0Data*> properties_;
};
};