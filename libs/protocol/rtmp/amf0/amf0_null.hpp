#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0Null : public Amf0Data {
public:
    static const AMF0_MARKER_TYPE marker = NULL_MARKER;
    Amf0Null() : Amf0Data(NULL_MARKER) {}
    virtual ~Amf0Null() {}
public:
    int32_t decode(const uint8_t *data, size_t len) {
        uint8_t *data_start = (uint8_t*)data;
        if(len < 1) {
            return -1;
        }

        auto marker = data[0];
        len--;
        data++;

        if (marker != NULL_MARKER) {
            return -2;
        }
        return data - data_start;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        uint8_t *data = buf;
        if (len < 1) {
            return -1;
        }
        // marker
        *data = NULL_MARKER;
        data++;
        return data - buf;
    }

    size_t size() const {
        return 1;
    }
};
};