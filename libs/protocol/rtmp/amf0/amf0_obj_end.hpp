#pragma once
#include "amf0_def.hpp"
namespace mms {
class Amf0ObjEnd : public Amf0Data {
public:
    static const AMF0_MARKER_TYPE marker = OBJECT_END_MARKER;
    Amf0ObjEnd() : Amf0Data(OBJECT_END_MARKER) {}
    virtual ~Amf0ObjEnd() {}
public:
    int32_t decode(const uint8_t *data, size_t len) {
        if (len < 3) {
            return -1;
        }
        //UTF-8-empty = U16 ; byte-length reported as zero with ; no UTF8-char content, i.e. 0x0000
        //object-end-type = UTF-8-empty object-end-marker
        if (data[0] != 0x00 || data[1] != 0x00 || data[2] != OBJECT_END_MARKER) {
            return -2;
        }

        return 3;
    }

    int32_t encode(uint8_t *buf, size_t len) const {
        uint8_t *data = buf;
        if (len < 3) {
            return -1;
        }
        // marker
        *data = 0x00;
        data++;
        *data = 0x00;
        data++;
        *data = OBJECT_END_MARKER;
        data++;
        return data - buf;
    }

    size_t size() const {
        return 3;
    }
};
};