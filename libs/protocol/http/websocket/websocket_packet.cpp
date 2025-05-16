/*
 * @Author: jbl19860422
 * @Date: 2023-10-03 12:24:28
 * @LastEditTime: 2023-10-07 20:40:59
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\websocket\websocket_packet.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <string.h>
#include <arpa/inet.h>
#include "websocket_packet.hpp"

using namespace mms;

WebSocketPacket::WebSocketPacket() {

}

WebSocketPacket::~WebSocketPacket() {

}

int32_t WebSocketPacket::decode(const char *data, int32_t len) {
    if (len < 2) {
        return 0;
    }

    const char *data_start = data;
    fin_ = (data[0]>>7)&0x01;
    opcode_ = (OpCode)((data[0])&0x0F);
    mask_ = (data[1]>>7)&0x01;
    payload_len_ = (data[1])&0x7f;

    len -= 2;
    data += 2;

    if (payload_len_ == 126) {//增加额外2字节表示长度
        if (len < 2) {
            return 0;
        }
        payload_len_ = ntohs(*(uint16_t*)data);
        data += 2;
        len -= 2;
    } else if (payload_len_ == 127) {//增加额外8字节表示长度
        if (len < 8) {
            return 0;
        }
        payload_len_ = be64toh(*(uint64_t*)data);
        data += 8;
        len -= 8;
    }

    if (mask_) {
        if (len < 4) {
            return 0;
        }
        masking_key_ = *(uint32_t*)data;
        data += 4;
        len -= 4;
    }

    if (len < payload_len_) {
        return 0;
    }

    payload_ = std::make_unique<char[]>(payload_len_);
    if (mask_) {
        char *p = (char*)&masking_key_;
        for (int i = 0; i < payload_len_; i++) {
            payload_.get()[i] = data[i] ^ p[i%4];
        }
    } else {
        memcpy(payload_.get(), data, payload_len_);
    }
    
    data += payload_len_;
    return data - data_start;
}

void WebSocketPacket::set_masking_key(uint32_t k) {
    mask_ = true;
    masking_key_ = k;
}

void WebSocketPacket::set_payload(std::unique_ptr<char[]> payload, int32_t payload_len) {
    payload_ = std::move(payload);
    payload_len_ = payload_len;
}

int32_t WebSocketPacket::encode(char *data, int32_t len) {
    const char *data_start = data;
    if (len < 2) {
        return -1;
    }

    data[0] = (fin_<<7) | (opcode_&0x0F);
    if (payload_len_ <= 125) {
        data[1] = (mask_<<7) | payload_len_;
        data += 2;
        len -= 2;
    } else if (payload_len_ <= 0xFFFF) {
        data[1] = (mask_<<7) | 126;
        data += 2;
        len -= 2;
        if (len < 2) {
            return -2;
        }
        *(uint16_t*)data = htons(payload_len_);
        data += 2;
        len -= 2;
    } else {
        data[1] = (mask_<<7) | 127;
        data += 2;
        len -= 2;
        if (len < 8) {
            return -3;
        }
        *(uint64_t*)data = htobe64(payload_len_);
        data += 8;
        len -= 8;
    }

    if (mask_) {
        if (len < 4) {
            return -4;
        }

        memcpy(data, &masking_key_, 4);
        data += 4;
        len -= 4;
    }

    if (len < payload_len_) {
        return -5;
    }

    memcpy(data, payload_.get(), payload_len_);
    data += payload_len_;
    len -= payload_len_;
    return data - data_start;
}

void WebSocketPacket::set_opcode(OpCode opcode) {
    opcode_ = opcode;
}

int32_t WebSocketPacket::encode_header(char *data, int32_t len) {
    const char *data_start = data;
    if (len < 2) {
        return -1;
    }

    data[0] = (fin_<<7) | (opcode_&0x0F);
    if (payload_len_ <= 125) {
        data[1] = (mask_<<7) | payload_len_;
        data += 2;
        len -= 2;
    } else if (payload_len_ <= 0xFFFF) {
        data[1] = (mask_<<7) | 126;
        data += 2;
        len -= 2;
        if (len < 2) {
            return -2;
        }
        *(uint16_t*)data = htons(payload_len_);
        data += 2;
        len -= 2;
    } else {
        data[1] = (mask_<<7) | 127;
        data += 2;
        len -= 2;
        if (len < 8) {
            return -3;
        }
        *(uint64_t*)data = htobe64(payload_len_);
        data += 8;
        len -= 8;
    }

    if (mask_) {
        if (len < 4) {
            return -4;
        }

        memcpy(data, &masking_key_, 4);
        data += 4;
        len -= 4;
    }
    return data - data_start;
}

int32_t WebSocketPacket::encode_header(char *data, int32_t len, OpCode opcode, bool fin, bool mask_flag, uint32_t mask_key, int32_t payload_len) {
    const char *data_start = data;
    if (len < 2) {
        return -1;
    }

    data[0] = (fin<<7) | (opcode&0x0F);
    if (payload_len <= 125) {
        data[1] = (mask_flag<<7) | payload_len;
        data += 2;
        len -= 2;
    } else if (payload_len <= 0xFFFF) {
        data[1] = (mask_flag<<7) | 126;
        data += 2;
        len -= 2;
        if (len < 2) {
            return -2;
        }
        *(uint16_t*)data = htons(payload_len);
        data += 2;
        len -= 2;
    } else {
        data[1] = (mask_flag<<7) | 127;
        data += 2;
        len -= 2;
        if (len < 8) {
            return -3;
        }
        *(uint64_t*)data = htobe64(payload_len);
        data += 8;
        len -= 8;
    }

    if (mask_flag) {
        if (len < 4) {
            return -4;
        }

        memcpy(data, &mask_key, 4);
        data += 4;
        len -= 4;
    }
    return data - data_start;
}

int32_t WebSocketPacket::header_size(int32_t payload_size) {
    int32_t len = 2;
    if (payload_size <= 125) {
        len += 0;
    } else if (payload_size <= 0xFFFF) {
        len += 2;
    } else {
        len += 8;
    }

    if (mask_) {
        len += 4;
    }

    return len;
}

int32_t WebSocketPacket::size() {
    return 2 + (mask_?4:0) + payload_len_;
}

std::string_view WebSocketPacket::get_payload() {
    return std::string_view(payload_.get(), payload_len_);
}