#pragma once
#include "rtp_packet.h"
#include "h264_rtp_pkt_info.h"
namespace mms {
// enum H264_RTP_HEADER_TYPE {
//     H264_RTP_PAYLOAD_UNKNOWN             = 0,
//     H264_RTP_PAYLOAD_SINGLE_NALU_START   = 1,
//     H264_RTP_PAYLOAD_SINGLE_NALU_END     = 23,
//     H264_RTP_PAYLOAD_STAP_A              = 24,
//     H264_RTP_PAYLOAD_STAP_B              = 25,
//     H264_RTP_PAYLOAD_MTAP16              = 26,
//     H264_RTP_PAYLOAD_MTAP32              = 27,
//     H264_RTP_PAYLOAD_FU_A                = 28,
//     H264_RTP_PAYLOAD_FU_B                = 29,
// };

class H264RtpPacket : public RtpPacket {
public:
    H264RtpPacket();
    virtual ~H264RtpPacket();
public:
    int32_t decode(uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
    size_t size();
    H264_RTP_HEADER_TYPE get_type() const {
        return type;
    }

    bool is_single_nalu() const {
        return type >= H264_RTP_PAYLOAD_SINGLE_NALU_START && type <= H264_RTP_PAYLOAD_SINGLE_NALU_END;
    }
    
    uint8_t get_nalu_type() const {
        return nalu_type;
    }

    uint8_t is_start_fu() const {
        return start_bit;
    }

    uint8_t is_end_fu() const {
        return end_bit;
    }
private:
    uint8_t marker;
    uint8_t nalu_type;
    H264_RTP_HEADER_TYPE type;
    // when fu-a
    uint8_t start_bit = 0;
    uint8_t end_bit = 0;
};

};