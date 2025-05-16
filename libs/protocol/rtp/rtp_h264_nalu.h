#pragma once
#include <map>
#include <memory>
#include "protocol/rtp/rtp_h264_packet.h"
#include "codec/h264/h264_nalu.h"
namespace mms {
class RtpH264NALU : public H264NALU {
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

    void set_stap_a(bool v) {
        stap_a_ = v;
    }


    bool is_stap_a() {
        return stap_a_;
    }
    
    uint16_t get_first_seqno();
    uint16_t get_last_seqno();

    std::map<uint16_t, std::shared_ptr<RtpPacket>> & get_rtp_pkts() {
        return rtp_pkts_;
    }

    bool is_last_nalu();
private:
    bool stap_a_ = false;
    std::map<uint16_t, std::shared_ptr<RtpPacket>> rtp_pkts_;
};
};