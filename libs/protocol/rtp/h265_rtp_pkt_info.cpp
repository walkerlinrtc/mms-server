#include "stdio.h"
#include "spdlog/spdlog.h"
#include "h265_rtp_pkt_info.h"
using namespace mms;
H265RtpPktInfo::H265RtpPktInfo() {

}

H265RtpPktInfo::~H265RtpPktInfo() {

}

int32_t H265RtpPktInfo::parse(const char *data, size_t len) {
    if (len < 3)
    {
        return -1;
    }

    type = H265_RTP_HEADER_TYPE((data[0]>>1) & 0x3F);  
    if (H264_RTP_PAYLOAD_FU == type)// fu_indicator(1byte) + fu_header(1byte) + rbsp
    {
        uint8_t fu_header  = data[2];
        start_bit  = (fu_header >> 7) & 0x01;
        end_bit    = (fu_header >> 6) & 0x01;
        nalu_type  = (fu_header & 0x3F);
        return 3;
    } 
    else if (H265_RTP_PAYLOAD_PACI == type) //暂时不支持PACI
    {
        return -1;
    } else if (H264_RTP_PAYLOAD_AGGREGATION_PACKET == type) {
    } else if (type >= H265_RTP_PAYLOAD_SINGLE_NALU_START && type <= H265_RTP_PAYLOAD_SINGLE_NALU_END) {
        nalu_type  = type;
    }

    return 2;
}