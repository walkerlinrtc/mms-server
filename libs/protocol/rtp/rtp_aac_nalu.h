#pragma once
#include <map>
#include <memory>

#include "rtp_packet.h"

namespace mms {
class RtpAACNALU {
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

private:
    std::map<uint16_t, std::shared_ptr<RtpPacket>> rtp_pkts_;
};
};