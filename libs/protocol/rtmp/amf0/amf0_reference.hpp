#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0Reference : public Amf0Data {
public:
    using value_type = uint16_t;
    static const AMF0_MARKER_TYPE marker = REFERENCE_MARKER;
public:
    Amf0Reference() : Amf0Data(REFERENCE_MARKER) {}
    int32_t decode(const uint8_t *data, size_t len) {
        int pos = 0;
        if(len < 1) {
            return -1;
        }

        auto marker = data[pos];
        len--;
        pos++;

        if (marker != REFERENCE_MARKER) {
            return -2;
        }

        if (len < 2) {
            return -3;
        }
        
        const uint8_t *d = data + pos;
        char *p = (char*)&obj_index_;
        p[0] = d[1];
        p[1] = d[0];
        pos += 2;
        len -= 2;
        return pos;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        ((void)buf);
        ((void)len);
        return 0;
    }

    uint16_t & get_value() {
        return obj_index_;
    }

    size_t size() const {
        return 3;
    }
private:
    uint16_t obj_index_;
};
};