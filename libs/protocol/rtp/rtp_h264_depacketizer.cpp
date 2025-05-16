#include "spdlog/spdlog.h"
#include "rtp_h264_depacketizer.h"
#include <string_view>
using namespace mms;

std::shared_ptr<RtpH264NALU> RtpH264Depacketizer::on_packet(std::shared_ptr<RtpPacket> pkt)
{
    H264RtpPktInfo pkt_info;
    pkt_info.parse(pkt->get_payload().data(), pkt->get_payload().size());
    // printf("h264 rtp type:%02x, nalu type:%02x, stap:%d, seqno:%d, timestamp:%u, marker:%d, single:%d, start:%d, end:%d\n", pkt_info.get_type(), pkt_info.get_nalu_type(), pkt_info.is_stap_a(), pkt->get_seq_num(), pkt->get_timestamp(), (uint32_t)pkt->get_header().marker, pkt_info.is_single_nalu(), pkt_info.is_start_fu(), pkt_info.is_end_fu());
    // if (pkt_info.is_single_nalu())
    // {// todo extrace h264 single nalu
    //     std::map<uint16_t, std::shared_ptr<RtpPacket>> m;
    //     m.insert(std::pair(pkt->get_seq_num(), pkt));
    //     std::shared_ptr<RtpH264NALU> out_pkt = std::make_shared<RtpH264NALU>();
    //     out_pkt->set_rtp_pkts(m);
    //     return out_pkt;
    // } else if (pkt_info.is_stap_a()) {
    //     std::map<uint16_t, std::shared_ptr<RtpPacket>> m;
    //     m.insert(std::pair(pkt->get_seq_num(), pkt));
    //     std::shared_ptr<RtpH264NALU> out_pkt = std::make_shared<RtpH264NALU>();
    //     out_pkt->set_rtp_pkts(m);
    //     out_pkt->set_stap_a(true);
    //     return out_pkt;
    // }

    auto & pkts_map = time_rtp_pkts_buf_[pkt->get_timestamp()];
    pkts_map.insert(std::pair(pkt->get_seq_num(), pkt));
    if (pkt->get_header().marker == 1) {
        std::shared_ptr<RtpH264NALU> out_pkt = std::make_shared<RtpH264NALU>();
        out_pkt->set_rtp_pkts(pkts_map);
        time_rtp_pkts_buf_.erase(pkt->get_timestamp());
        return out_pkt;
    }
    return nullptr;

    // auto it = pkts_map.begin();

    // pkt_info.parse(it->second->get_payload().data(), it->second->get_payload().size());
    // if (!pkt_info.is_start_fu())
    // {
    //     return nullptr;
    // }

    // bool find_end_fu = false;
    // uint16_t prev_seq = it->second->get_seq_num();
    // it++;
    // while (it != pkts_map.end())
    // {
    //     if (it->second->get_seq_num() != prev_seq + 1)
    //     {
    //         return nullptr;
    //     }
        
    //     pkt_info.parse(it->second->get_payload().data(), it->second->get_payload().size());
    //     if (pkt_info.is_end_fu())
    //     {
    //         find_end_fu = true;
    //     }

    //     it++;
    // }

    // if (!find_end_fu)
    // {
    //     return nullptr;
    // }
    // // 条件都具备了，输出H264NALU
    // std::shared_ptr<RtpH264NALU> out_pkt = std::make_shared<RtpH264NALU>();
    // printf("get a nalu pkts count:%d\n", pkts_map.size());
    // out_pkt->set_rtp_pkts(pkts_map);
    // return out_pkt;

    // out_pkt->rbsp.resize(total_nalu_size);
    // size_t pos = 0;
    // for (it = pkts_map.begin(); it != pkts_map.end(); it++)
    // {
    //     memcpy((char*)out_pkt->rbsp.data() + pos, it->second->payload_ + 2, it->second->payload_len_ - 2);
    //     pos += it->second->payload_len_ - 2;
    // }
    // return out_pkt;
}