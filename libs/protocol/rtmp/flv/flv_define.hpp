/*
MIT License

Copyright (c) 2021 jiangbaolin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <memory>
#include <list>

#include "rtmp_define.hpp"
namespace mms {
#define FLV_IDENTIFIER  "FLV"
#define FLV_TAG_HEADER_BYTES 11

#pragma pack(1)
struct FlvHeader {
    uint8_t signature[3] = {'F', 'L', 'V'};
    uint8_t version = 0x01;
    union FlagUnion {
        struct {
            uint8_t reserved1 :5;
            uint8_t audio     :1;
            uint8_t reserved2 :1;
            uint8_t video     :1;
        } flags;
        uint8_t value;
    };

    FlagUnion flag;
    uint32_t data_offset = 0x09;

    int32_t encode(uint8_t *data, size_t len) {
        if (len < 9) {
            return 0;
        }
        
        data[0] = 'F';
        data[1] = 'L';
        data[2] = 'V';
        data[3] = 0x01;
        data[4] = (uint8_t)flag.value;
        data[5] = 0x00;
        data[6] = 0x00;
        data[7] = 0x00;
        data[8] = 0x09;
        return 9;
    }

    int32_t decode(uint8_t *data, size_t len) {
        if (len < 9) {
            return 0;
        }

        if (data[0] != 'F' || data[1] != 'L' || data[2] != 'V') {
            return -1;
        }
        version = data[3];
        flag.value = data[4];
        if (data[8] != 0x09) {
            return -2;
        }
        return 9;
    }
};

struct FlvTagHeader {
    enum TagType : uint8_t {
        AudioTag    = 8,
        VideoTag    = 9,
        ScriptTag   = 18,
    };

    // struct Type {
    //     uint8_t reserved:2;
    //     uint8_t filter:1;
    //     TagType type:5;
    // };
    
    uint8_t tag_type;
    uint32_t data_size = 0;//3byte
    uint32_t timestamp;//4byte
    uint32_t stream_id;//3byte

    // static int32_t encodeFromRtmpMessage(std::shared_ptr<RtmpMessage> msg, uint8_t *data, size_t len) {
    //     uint8_t *start = data;
    //     if (len < 1) {
    //         return -1;
    //     }

    //     *data = msg->message_type_id_;
    //     len--;
    //     data++;

    //     if (len < 3) {
    //         return -2;
    //     }

    //     uint8_t *p = (uint8_t*)(&msg->payload_size_);
    //     data[0] = p[2];
    //     data[1] = p[1];
    //     data[2] = p[0];
    //     data += 3;
    //     len -= 3;
    //     if (len < 4) {
    //         return -3;
    //     }

    //     p = (uint8_t*)(&msg->timestamp_);
    //     data[0] = p[2];
    //     data[1] = p[1];
    //     data[2] = p[0];
        
    //     data[3] = p[3];
    //     data += 4;
    //     len -= 4;

    //     if (len < 3) {
    //         return -4;
    //     }
    //     data[0] = 0;
    //     data[1] = 0;
    //     data[2] = 0;
    //     data += 3;
    //     len -= 3;
    //     return data - start;
    // }

    int32_t encode(uint8_t *data, size_t len) {
        uint8_t *data_start = (uint8_t*)data;
        if (len < 1) {
            return -1;
        }
        *data = (uint8_t)tag_type;
        data++;
        len--;

        if (len < 3) {
            return -2;
        }
        uint8_t *p = (uint8_t*)&data_size;
        data[0] = p[2];
        data[1] = p[1];
        data[2] = p[0];
        data += 3;
        len -= 3;

        if (len < 4) {
            return -3;
        }
        p = (uint8_t*)&timestamp;
        data[0] = p[2];
        data[1] = p[1];
        data[2] = p[0];
        data[3] = p[3];
        data += 4;
        len -= 4;

        if (len < 3) {
            return -4;
        }
        p = (uint8_t*)&stream_id;
        data[0] = p[2];
        data[1] = p[1];
        data[2] = p[0];
        data += 3;
        return data - data_start;
    }

    int32_t decode(const uint8_t *data, size_t len) {
        uint8_t *data_start = (uint8_t*)data;
        if (len < 1) {
            return 0;
        }
        tag_type = data[0];
        len--;
        data++;
        
        if (len < 3) {
            return 0;
        }
        // uint8_t *p = (uint8_t*)&data_size;
        uint8_t t[4] = {0};
        memcpy(t + 1, data, 3);
        data_size = ntohl(*(uint32_t*)t);
        data += 3;
        len -= 3;

        if (len < 4) {
            return 0;
        }
        memset(t, 0, 4);
        memcpy(t + 1, data, 3);
        timestamp = ntohl(*(uint32_t*)t);
        timestamp |= ((uint32_t)data[3]) << 24;
        data += 4;
        len -= 4;

        if (len < 3) {
            return 0;
        }
        
        uint8_t *p = (uint8_t *)&stream_id;
        p[0] = data[2];
        p[1] = data[1];
        p[2] = data[0];
        data += 3;
        return data - data_start;
    }
};



#pragma pack()

};