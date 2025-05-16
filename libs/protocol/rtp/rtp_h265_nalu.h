#pragma once
#include <map>
#include <memory>
#include "protocol/rtp/rtp_h264_packet.h"
#include "codec/hevc/hevc_nalu.h"
namespace mms {
class RtpH265NALU : public HEVCNALU {
public:
    void set_rtp_pkts(const std::map<uint16_t, std::shared_ptr<RtpPacket>> & pkts) 
    {
        rtp_pkts_ = pkts;
    }

    size_t get_pkts_count() {
        return rtp_pkts_.size();
    }

    uint32_t get_timestamp() {
        auto it = rtp_pkts_.begin();
        if (it != rtp_pkts_.end()) {
            return it->second->get_timestamp();
        }
        return 0;
    }
    
    uint16_t get_first_seqno();
    uint16_t get_last_seqno();

    std::map<uint16_t, std::shared_ptr<RtpPacket>> & get_rtp_pkts() {
        return rtp_pkts_;
    }

    bool is_last_nalu();
private:
    std::map<uint16_t, std::shared_ptr<RtpPacket>> rtp_pkts_;
};
};