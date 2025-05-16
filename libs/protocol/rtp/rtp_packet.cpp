#include <iostream>
#include "rtp_packet.h"
using namespace mms;

RtpPacket::~RtpPacket()
{
    if (payload_)
    {
        delete[] payload_;
        payload_ = nullptr;
        payload_len_ = 0;
    }
}
int32_t RtpPacket::decode_and_store(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = header_.decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }

    data += consumed;
    len -= consumed;

    payload_ = new char[len];
    memcpy(payload_, data, len);
    payload_len_ = len;
    data += len;
    len = 0;
    return data - data_start;
}

int32_t RtpPacket::encode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = header_.encode(data, len);
    if (consumed < 0) {
        return -1;
    }

    data += consumed;
    len -= consumed;
    if (len < payload_len_) {
        return -2;
    }
    memcpy(data, payload_, payload_len_);
    data += payload_len_;
    return data - data_start;
}

size_t RtpPacket::size()
{
    return 0;
}

std::string_view RtpPacket::get_payload() 
{
    return std::string_view(payload_, payload_len_);
}

uint16_t RtpPacket::get_seq_num()
{
    return header_.seqnum;
}

uint32_t RtpPacket::get_timestamp()
{
    return header_.timestamp;
}