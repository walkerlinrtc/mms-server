#include "stdio.h"
#include "spdlog/spdlog.h"
#include "h264_rtp_pkt_info.h"
using namespace mms;
H264RtpPktInfo::H264RtpPktInfo() {

}

H264RtpPktInfo::~H264RtpPktInfo() {

}

int32_t H264RtpPktInfo::parse(const char *data, size_t len) {
    if (len < 2)
    {
        return -1;
    }

    type = (H264_RTP_HEADER_TYPE)(data[0] & 0x1F);  
    if (H264_RTP_PAYLOAD_FU_A == type)// fu_indicator(1byte) + fu_header(1byte) + rbsp
    {
        uint8_t fu_header  = data[1];
        start_bit  = (fu_header >> 7) & 0x01;
        end_bit    = (fu_header >> 6) & 0x01;
        nalu_type  = (fu_header & 0x1F);
        return 2;
    } 
    else if (H264_RTP_PAYLOAD_STAP_A == type) 
    {
        return 1;
    } 
    else if (type >= H264_RTP_PAYLOAD_SINGLE_NALU_START && type <= H264_RTP_PAYLOAD_SINGLE_NALU_END)
    {
        nalu_type  = type;
    }

    return 1;
}

// int32_t H264RtpPktInfo::encode(uint8_t *data, size_t len) {
//     if (len < 2) {
//         return -1;
//     }
//     *data = (type & 0x1f);
//     if (H264_RTP_PAYLOAD_FU_A == type) {
//         data++;
//         *data = (start_bit << 7) | (end_bit << 6) | nalu_type;
//         return 2;
//     }
//     return 1;
// }

// size_t H264RtpPktInfo::size() {
//     if (H264_RTP_PAYLOAD_FU_A == type) {
//         return 2;
//     }
//     return 1;
// }