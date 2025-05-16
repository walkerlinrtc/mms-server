#include <iostream>
#include "rtp_h264_packet.h"
using namespace mms;

H264RtpPacket::H264RtpPacket()
{

}

H264RtpPacket::~H264RtpPacket()
{

}

int32_t H264RtpPacket::decode(uint8_t *data, size_t len)
{
    int32_t consumed = RtpPacket::decode_and_store(data, len);
    if (payload_len_ < 1)
    {
        return -1;
    }

    type = (H264_RTP_HEADER_TYPE)(payload_[0] & 0x1F);    
    if (H264_RTP_PAYLOAD_FU_A == type)
    {
        uint8_t fu_header  = payload_[1];
        start_bit  = (fu_header >> 7) & 0x01;
        end_bit    = (fu_header >> 6) & 0x01;
        nalu_type  = (fu_header & 0x1F);
    } 
    else if (type >= H264_RTP_PAYLOAD_SINGLE_NALU_START && type <= H264_RTP_PAYLOAD_SINGLE_NALU_END)
    {
        nalu_type  = type;
    }

    return consumed;
}

int32_t H264RtpPacket::encode(uint8_t *data, size_t len)
{
    int32_t consumed = RtpPacket::encode(data, len);
    return consumed;
}


size_t H264RtpPacket::size()
{
    return 0;
}