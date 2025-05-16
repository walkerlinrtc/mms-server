#include "amf0_ecma_array.hpp"
#include "amf0_object.hpp"
using namespace mms;

Json::Value Amf0EcmaArray::to_json() {
    Json::Value root;
    for (auto & p : properties_) {
        switch(p.second->get_type()) {
            case NUMBER_MARKER: {
                root[p.first] = ((Amf0Number*)p.second)->get_value();
                break;
            }
            case BOOLEAN_MARKER: {
                root[p.first] = ((Amf0Boolean*)p.second)->get_value();
                break;
            }
            case STRING_MARKER: {
                root[p.first] = ((Amf0String*)p.second)->get_value();
                break;
            }
            case OBJECT_MARKER: {
                root[p.first] = ((Amf0Object*)p.second)->to_json();
                break;
            }
            case NULL_MARKER: {
                root[p.first] = nullptr;
                break;
            }
            case UNDEFINED_MARKER: {
                root[p.first] = "undefined";
                break;
            }
            default : {
                
            }
        }
    }
    return root;
}

int32_t Amf0EcmaArray::decode(const uint8_t *data, size_t len) {
    auto buf_start = data;
    if (len < 1) {
        
        return -1;
    }
    auto marker = *data;
    if (marker != ECMA_ARRAY_MARKER) {
        return -2;
    }
    data++;
    len--;

    if (len < 4) {
        return -3;
    }

    int32_t count = 0;
    char *p = (char*)&count;
    p[0] = data[3];
    p[1] = data[2];
    p[2] = data[1];
    p[3] = data[0];

    // count = ntohl(*(uint32_t*)data);

    data += 4;
    len -= 4;
    while (count > 0) {
        // read key
        if (len < 2) {
            return -4;
        }
        uint16_t key_len = 0;
        char *p = (char*)&key_len;
        p[0] = data[1];
        p[1] = data[0];

        data += 2;
        len -= 2;

        if (len < key_len) {
            return -5;
        }

        std::string key;
        key.assign((const char *)data, key_len);
        data += key_len;
        len -= key_len;
        // read marker
        if (len < 1) {
            return -6;
        }
    
        AMF0_MARKER_TYPE marker = (AMF0_MARKER_TYPE)(*data);
        Amf0Data *value = nullptr;
        switch(marker) {
            case NUMBER_MARKER:{
                value = new Amf0Number;
                break;
            }
            case BOOLEAN_MARKER:{
                value = new Amf0Boolean;
                break;
            }
            case STRING_MARKER:{
                value = new Amf0String;
                break;
            }
            case NULL_MARKER:{
                value = new Amf0Null;
                break;
            }
            case UNDEFINED_MARKER:{
                value = new Amf0Undefined;
                break;
            }
            case OBJECT_MARKER:{
                value = new Amf0Object;
                break;
            }
            default : {
                return -7;
            }
        }

        if (value != nullptr) {
            int32_t consumed = value->decode(data, len);
            if (consumed < 0) {
                delete value;
                return -8;
            }
            len -= consumed;
            data += consumed;
            auto it = properties_.find(key);
            if (it != properties_.end()) {
                delete it->second;
            }
            properties_[key] = value;
        }
        count--;
    }

    Amf0ObjEnd end;
    auto consumed = end.decode(data, len);
    if (consumed < 0) {
        return -8;
    }
    data += consumed;
    len -= consumed;
    return data - buf_start;
}

int32_t Amf0EcmaArray::encode(uint8_t *buf, size_t len) const {
    uint8_t *data = buf;
    if (len < 1) {
        return -1;
    }
    // marker
    *data = OBJECT_MARKER;
    data++;
    len--;
    
    for (auto & p : properties_) {
        // key
        if (len < 2) {
            return -3;
        }
        *(uint16_t*)data = htons(p.first.size());
        data += 2;
        len -= 2;

        memcpy(data, p.first.data(), p.first.size());
        data += p.first.size();
        len -= p.first.size();

        int32_t consumed = p.second->encode(data, len);
        if (consumed < 0) {
            return -3;
        }
        data += consumed;
        len -= consumed;
    }

    Amf0ObjEnd end;
    if (len < end.size()) {
        return -4;
    }
    int32_t consumed = end.encode(data, len);
    data += consumed;
    len -= consumed;
    return data - buf;
}