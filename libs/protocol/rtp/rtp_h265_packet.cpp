#include <iostream>
#include "rtp_h265_packet.h"
using namespace mms;

H265RtpPacket::H265RtpPacket()
{

}

H265RtpPacket::~H265RtpPacket()
{

}

int32_t H265RtpPacket::decode(uint8_t *data, size_t len)
{
    int32_t consumed = RtpPacket::decode_and_store(data, len);
    if (payload_len_ < 3)
    {
        return -1;
    }

    type = (H265_RTP_HEADER_TYPE)((payload_[0]>>1) & 0x3F);    
    if (H264_RTP_PAYLOAD_FU == type)
    {
        uint8_t fu_header  = payload_[2];
        start_bit  = (fu_header >> 7) & 0x01;
        end_bit    = (fu_header >> 6) & 0x01;
        nalu_type  = (fu_header & 0x1F);
    } 
    else if (type >= H265_RTP_PAYLOAD_SINGLE_NALU_START && type <= H265_RTP_PAYLOAD_SINGLE_NALU_END)
    {
        nalu_type  = type;
    }

    return consumed;
}

int32_t H265RtpPacket::encode(uint8_t *data, size_t len)
{
    int32_t consumed = RtpPacket::encode(data, len);
    return consumed;
}


size_t H265RtpPacket::size()
{
    return 0;
}