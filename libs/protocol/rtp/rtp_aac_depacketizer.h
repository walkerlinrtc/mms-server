#pragma once
#include <memory>
#include <unordered_map>
#include <map>

#include "rtp_au.h"
#include "rtp_packet.h"
#include "rtp_aac_nalu.h"
namespace mms {
class RtpPacket;
class RtpAACDepacketizer {
public:
    RtpAACDepacketizer(){};
    virtual ~RtpAACDepacketizer() {};
    std::shared_ptr<RtpAACNALU> on_packet(std::shared_ptr<RtpPacket> pkt);
private:
    std::unordered_map<uint32_t, std::map<uint16_t, std::shared_ptr<RtpPacket>>> time_rtp_pkts_buf_;
};
};