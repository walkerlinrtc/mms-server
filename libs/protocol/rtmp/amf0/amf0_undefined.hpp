#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0Undefined : public Amf0Data {
public:
    static const AMF0_MARKER_TYPE marker = UNDEFINED_MARKER;
    Amf0Undefined() : Amf0Data(UNDEFINED_MARKER){}
    virtual ~Amf0Undefined() {}
public:
    int32_t decode(const uint8_t *data, size_t len) {
        int pos = 0;
        if(len < 1) {
            return -1;
        }

        auto marker = data[0];
        len--;
        pos++;
        data++;

        if (marker != UNDEFINED_MARKER) {
            return -2;
        }
        return pos;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        uint8_t *data = buf;
        if (len < 1) {
            return -1;
        }
        // marker
        *data = UNDEFINED_MARKER;
        data++;
        len--;
        return data - buf;
    }

    size_t size() const {
        return 1;
    }
};
};