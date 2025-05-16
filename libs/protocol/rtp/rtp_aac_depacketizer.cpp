#include "rtp_aac_depacketizer.h"
using namespace mms;

std::shared_ptr<RtpAACNALU> RtpAACDepacketizer::on_packet(std::shared_ptr<RtpPacket> pkt) {
    auto & pkts_map = time_rtp_pkts_buf_[pkt->get_timestamp()];
    pkts_map.insert(std::pair(pkt->get_seq_num(), pkt));
    if (pkt->get_header().marker == 1) {
        std::shared_ptr<RtpAACNALU> out_pkt = std::make_shared<RtpAACNALU>();
        out_pkt->set_rtp_pkts(pkts_map);
        time_rtp_pkts_buf_.erase(pkt->get_timestamp());
        return out_pkt;
    }
    return nullptr;
}