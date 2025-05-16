#pragma once
#include "rtp_packet.h"
#include "h264_rtp_pkt_info.h"
namespace mms {
enum H265_RTP_HEADER_TYPE {
    H264_RTP_PAYLOAD_AGGREGATION_PACKET     = 48,
    H264_RTP_PAYLOAD_FU                     = 49,
    H265_RTP_PAYLOAD_PACI                   = 50,
    H265_RTP_PAYLOAD_SINGLE_NALU_START      = 0,
    H265_RTP_PAYLOAD_SINGLE_NALU_END        = 40
};
class H265RtpPacket : public RtpPacket {
public:
    H265RtpPacket();
    virtual ~H265RtpPacket();
public:
    int32_t decode(uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
    size_t size();
    H265_RTP_HEADER_TYPE get_type() const {
        return type;
    }

    bool is_single_nalu() const {
        return type >= H265_RTP_PAYLOAD_SINGLE_NALU_START && type <= H265_RTP_PAYLOAD_SINGLE_NALU_END;
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
    H265_RTP_HEADER_TYPE type;
    // when fu
    uint8_t start_bit = 0;
    uint8_t end_bit = 0;
};

};