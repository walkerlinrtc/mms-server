#pragma once

#include "amf0_def.hpp"
namespace mms {
class Amf0Number : public Amf0Data {
public:
    using value_type = double;
    static const AMF0_MARKER_TYPE marker = NUMBER_MARKER;

    Amf0Number() : Amf0Data(NUMBER_MARKER) {}
    virtual ~Amf0Number() {}
public:
    int32_t decode(const uint8_t *data, size_t len){
        int pos = 0;
        if(len < 1) {
            return -1;
        }

        auto marker = data[pos];
        len--;
        pos++;

        if (marker != NUMBER_MARKER) {
            return -2;
        }

        if (len < 8) {
            return -3;
        }
        
        const uint8_t *d = data + pos;
        uint8_t *p = (uint8_t*)&value_;
        // value_ = double(be64toh(*(uint64_t*)d));
        p[0] = d[7];
        p[1] = d[6];
        p[2] = d[5];
        p[3] = d[4];
        p[4] = d[3];
        p[5] = d[2];
        p[6] = d[1];
        p[7] = d[0];
        
        pos += 8;
        len --;
        return pos;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        uint8_t *data = buf;
        if (len < 1) {
            return -1;
        }
        // marker
        *data = NUMBER_MARKER;
        data++;
        len--;
        // len
        if (len < 8) {
            return -2;
        }
        uint8_t *d = (uint8_t *)&value_;
        data[0] = d[7];
        data[1] = d[6];
        data[2] = d[5];
        data[3] = d[4];
        data[4] = d[3];
        data[5] = d[2];
        data[6] = d[1];
        data[7] = d[0];

        data += 8;
        len -= 8;
        return data - buf;
    }

    double get_value() const {
        return value_;
    }

    void set_value(double d) {
        value_ = d;
    }

    size_t size() const {
        return 9;
    }

    double value_;
};
};