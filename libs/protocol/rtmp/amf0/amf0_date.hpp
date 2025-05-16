#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0Date : public Amf0Data {
public:
    using value_type = double;
    static const AMF0_MARKER_TYPE marker = DATE_MARKER;
    Amf0Date() : Amf0Data(DATE_MARKER) {}
public:
    double & get_value() {
        return utc_;
    }

    int32_t decode(const uint8_t *data, size_t len) {
        int pos = 0;
        if(len < 1) {
            return -1;
        }

        auto marker = data[0];
        len--;
        pos++;
        data++;

        if (marker != DATE_MARKER) {
            return -2;
        }

        if (len < 8) {
            return -3;
        }

        char *p = (char*)&utc_;
        p[0] = data[7];
        p[1] = data[6];
        p[2] = data[5];
        p[3] = data[4];
        p[4] = data[3];
        p[5] = data[2];
        p[6] = data[1];
        p[7] = data[0];

        data += 8;
        pos += 8;
        len -= 8;

        if (len < 2) {
            return -4;
        }

        p = (char*)&time_zone_;
        p[0] = data[1];
        p[1] = data[0];
        pos += 2;
        len -= 2;
        data += 2;
        return pos;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        ((void)buf);
        ((void)len);
        return 0;
    }

    size_t size() const {
        return 11;//1 + 8 + 2;
    }
public:
    double utc_;
    int16_t time_zone_;
};

};