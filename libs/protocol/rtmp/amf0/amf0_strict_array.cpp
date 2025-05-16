#include "amf0_strict_array.hpp"

#include "amf0_def.hpp"
#include "amf0_number.hpp"
#include "amf0_boolean.hpp"
#include "amf0_string.hpp"
#include "amf0_object.hpp"
#include "amf0_null.hpp"
#include "amf0_undefined.hpp"
#include "amf0_reference.hpp"

namespace mms {

int32_t Amf0StrictArray::decode(const uint8_t *data, size_t len) {
    size_t pos = 0;
    if (len < 1) {
        return -1;
    }

    auto marker = data[0];
    len--;
    pos++;
    data++;

    if (marker != STRICT_ARRAY_MARKER) {
        return -2;
    }

    if(len < 4) {
        return -3;
    }

    uint32_t array_count = 0;
    char *p = (char*)&array_count;
    p[0] = data[3];
    p[1] = data[2];
    p[2] = data[1];
    p[3] = data[0];
    pos += 4;
    len -= 4;
    data += 4;

    uint32_t i = 0;
    while(i < array_count && len > 0) {
        // read data
        if (len < 1) {
            return -4;
        }
        auto marker = (AMF0_MARKER_TYPE)data[0];
        // len--;
        // data++;
        // pos++;
        Amf0Data* value = nullptr;
        switch (marker) {
        case NUMBER_MARKER: {
            value = new Amf0Number;
            break;
        }
        case BOOLEAN_MARKER: {
            value = new Amf0Boolean;
            break;
        }
        case STRING_MARKER: {
            value = new Amf0String;
            break;
        }
        case OBJECT_MARKER: {
            value = new Amf0Object;
            break;
        }
        case NULL_MARKER: {
            value = new Amf0Null;
            break;
        }
        case UNDEFINED_MARKER: {
            value = new Amf0Undefined;
            break;
        }
        case REFERENCE_MARKER: {
            value = new Amf0Reference;
            break;
        }
        case STRICT_ARRAY_MARKER: {
            value = new Amf0StrictArray;
            break;
        }
        default: {
            return -5;
        }
        }

        if (value) {
            int32_t consumed = value->decode(data, len);
            if (consumed < 0) {
                delete value;
                return -6;
            }
            len -= consumed;
            pos += consumed;
            data += consumed;
            
            datas_.emplace_back(value);
            i++;
        }
    }
    return pos;
}

int32_t Amf0StrictArray::encode(uint8_t *buf, size_t len) const {
    uint8_t *data = buf;
    if (len < 1) {
        return -1;
    }
    // marker
    *data = STRICT_ARRAY_MARKER;
    data++;
    len--;

    if (len < 4) {
        return -2;
    }
    data += 4;
    len -= 4;
    
    for (size_t i = 0; i < datas_.size(); i++) {
        int32_t consumed = datas_[i]->encode(data, len);
        if (consumed < 0) {
            return -3;
        }
        data += consumed;
        len -= consumed;
    }

    return data - buf;
}


};