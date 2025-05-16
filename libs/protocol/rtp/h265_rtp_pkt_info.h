#pragma once
#include <stdint.h>
#include <stddef.h>
#include "rtp_h265_packet.h"

namespace mms {
class RtpPacket;

class H265RtpPktInfo {
public:
    H265RtpPktInfo();
    virtual ~H265RtpPktInfo();
public:
    int32_t parse(const char *data, size_t len);
    H265_RTP_HEADER_TYPE get_type() const {
        return type;
    }

    bool is_single_nalu() const {
        return type >= H265_RTP_PAYLOAD_SINGLE_NALU_START && type <= H265_RTP_PAYLOAD_SINGLE_NALU_END;
    }

    bool is_fu() const {
        return type == H264_RTP_PAYLOAD_FU;
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
    uint8_t nalu_type;
    H265_RTP_HEADER_TYPE type;
    // when fu-a
    uint8_t start_bit = 0;
    uint8_t end_bit = 0;
};

};