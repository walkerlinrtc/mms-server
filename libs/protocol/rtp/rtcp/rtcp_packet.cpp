#include <iostream>
#include "rtcp_packet.h"
using namespace mms;

RtcpPacket::~RtcpPacket()
{
    if (payload_)
    {
        delete payload_;
        payload_ = nullptr;
        payload_len_ = 0;
    }
}
int32_t RtcpPacket::decode_and_store(uint8_t *data, int32_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = header_.decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }

    data += consumed;
    len -= consumed;

    if (len != (((int32_t)header_.length << 2) - 8)) 
    {
        return -2;
    }

    payload_ = new char[len];
    memcpy(payload_, data, len);
    payload_len_ = len;
    data += len;
    len = 0;
    return data - data_start;
}

int32_t RtcpPacket::encode(uint8_t *data, int32_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = header_.encode(data, len);
    if (consumed < 0) {
        return -1;
    }

    data += consumed;
    len -= consumed;
    if (payload_len_ > 0) {
        if (len < (int32_t)payload_len_) {
            return -2;
        }
        memcpy(data, payload_, payload_len_);
        data += payload_len_;
    }
    
    return data - data_start;
}

size_t RtcpPacket::size()
{
    return 0;
}

std::string_view RtcpPacket::get_payload() 
{
    return std::string_view(payload_, payload_len_);
}