#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0Boolean : public Amf0Data {
public:
    using value_type = bool;
    static const AMF0_MARKER_TYPE marker = BOOLEAN_MARKER;
    Amf0Boolean() : Amf0Data(BOOLEAN_MARKER) {}
    virtual ~Amf0Boolean() {}
    int32_t decode(const uint8_t *data, size_t len) {
        int pos = 0;
        if(len < 1) {
            return -1;
        }

        auto marker = data[0];
        len--;
        pos++;
        data++;

        if (marker != BOOLEAN_MARKER) {
            return -2;
        }

        if (len < 1) {
            return -3;
        }
        
        if (data[0] > 0) {
            value_ = true;
        } else {
            value_ = false;
        }
        pos++;
        return pos;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        uint8_t *data = buf;
        if (len < 1) {
            return -1;
        }
        // marker
        *data = BOOLEAN_MARKER;
        data++;
        len--;
        // len
        if (len < 1) {
            return -2;
        }
        *data = value_;
        data++;
        len--;
        return data - buf;
    }

    const bool & get_value() {
        return value_;
    }

    void set_value(bool v) {
        value_ = v;
    }
    
    size_t size() const {
        return 2;
    }
private:
    bool value_;
};
};