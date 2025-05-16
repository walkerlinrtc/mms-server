#pragma once
#include <stdint.h>
#include "json/json.h"
namespace mms {
enum AMF0_MARKER_TYPE {
    NUMBER_MARKER       =   0x00,
    BOOLEAN_MARKER      =   0x01,
    STRING_MARKER       =   0x02,
    OBJECT_MARKER       =   0x03,
    MOVIECLIP_MARKER    =   0x04,//reserved, not supported 
    NULL_MARKER         =   0x05,
    UNDEFINED_MARKER    =   0x06,
    REFERENCE_MARKER    =   0x07,
    ECMA_ARRAY_MARKER   =   0x08,
    OBJECT_END_MARKER   =   0x09,
    STRICT_ARRAY_MARKER =   0x0a,
    DATE_MARKER         =   0x0B,
    LONG_STRING_MARKER  =   0x0C,
    UNSUPPORTED_MARKER  =   0x0E,//reserved, not supported 
    XML_DOCUMENT_MARKER =   0x0F,
    TYPED_OBJECT_MARKER =   0x10
};

class Amf0Data {
public:
    Amf0Data(AMF0_MARKER_TYPE type) : type_(type) {}
    virtual ~Amf0Data() {}
    inline AMF0_MARKER_TYPE & get_type() {
        return type_;
    }
    virtual int32_t decode(const uint8_t *data, size_t len) = 0;
    virtual int32_t encode(uint8_t *data, size_t len) const = 0;
    virtual size_t size() const {
        return 0;
    };

    virtual Json::Value to_json() {
        Json::Value r;
        return r;
    }
protected:
    AMF0_MARKER_TYPE type_;
};

};