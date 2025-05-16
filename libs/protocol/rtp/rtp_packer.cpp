//
// Created by hijiang on 2019/8/8.
//
#include <assert.h>
#include "rtp_packer.h"
#include "spdlog/spdlog.h"
#include "codec/hevc/hevc_define.hpp"

using namespace mms;
#define NAL_RTP_PACKET_SIZE 1400

RtpPacker::RtpPacker()
{

}

uint16_t RtpPacker::get_sequence_num()
{
    if (++rtp_seqnum_ >= 65535)
    {
        rtp_seqnum_ = 0;
    }
    return rtp_seqnum_;
}

std::vector<std::shared_ptr<RtpPacket>> RtpPacker::pack(char *data, int32_t len, uint8_t pt, int ssrc, int64_t pts)
{
    std::vector<std::shared_ptr<RtpPacket>> pkts;
    int32_t left_len = len;
    int curr_pos = 0;
    while (left_len > 0)
    {
        int consume_count = 0;
        std::shared_ptr<RtpPacket> pkt;
        if(left_len < NAL_RTP_PACKET_SIZE)
        {
            pkt = std::make_shared<RtpPacket>();
            pkt->header_.pt = pt;
            pkt->header_.timestamp = pts;
            pkt->header_.marker = 1;
            pkt->header_.seqnum = get_sequence_num();
            pkt->header_.ssrc = ssrc;
            pkt->payload_ = new char[left_len];
            memcpy(pkt->payload_, data + curr_pos, left_len);
            // pkt->payload_ = data + curr_pos;
            pkt->payload_len_ = left_len;
            consume_count = left_len;
        }
        else
        {
            pkt = std::make_shared<RtpPacket>();
            pkt->header_.pt = pt;
            pkt->header_.timestamp = pts;
            pkt->header_.marker = 0;
            pkt->header_.seqnum = get_sequence_num();
            pkt->header_.ssrc = ssrc;
            pkt->payload_ = new char[NAL_RTP_PACKET_SIZE];
            memcpy(pkt->payload_, data + curr_pos, NAL_RTP_PACKET_SIZE);
            // pkt->payload_ = data + curr_pos;
            pkt->payload_len_ = NAL_RTP_PACKET_SIZE;
            consume_count = NAL_RTP_PACKET_SIZE;
        }
        pkts.push_back(pkt);
        curr_pos += consume_count;
        left_len -= consume_count;
    }

    return pkts;
}

std::vector<std::shared_ptr<RtpPacket>> RtpPacker::pack(const std::list<std::string_view> & nalus, uint8_t pt, int ssrc, int64_t pts) {
    std::vector<std::shared_ptr<RtpPacket>> pkts;
    int i = 0;
    for (auto & nalu : nalus) {
        i++;
        bool last = i == (int)nalus.size();
        if (nalu.size() > NAL_RTP_PACKET_SIZE - 3) {// FU_A
            int32_t data_pos = 0;
            bool start = true;
            bool end = false; 
            char nalu_type = (*nalu.data());
            char *data = (char*)nalu.data() + 1;
            int32_t total_bytes = nalu.size() - 1;
            
            while (data_pos < total_bytes) {
                auto pkt = std::make_shared<RtpPacket>();
                pkt->header_.pt = pt;
                pkt->header_.timestamp = pts;
                pkt->header_.marker = 0;
                pkt->header_.seqnum = get_sequence_num();
                pkt->header_.ssrc = ssrc;
                int32_t left_bytes = total_bytes - data_pos;
                int32_t payload_len = (left_bytes > NAL_RTP_PACKET_SIZE)?NAL_RTP_PACKET_SIZE:left_bytes;
                pkt->payload_ = new char[payload_len+2];
                pkt->payload_len_ = payload_len + 2;
                if ((data_pos + payload_len) >= total_bytes) {
                    end = true;
                    if (last) {
                        pkt->header_.marker = 1;
                    }
                }
                
                pkt->payload_[0] = (nalu_type & (~0x1F)) | 28;
                pkt->payload_[1] = (nalu_type & 0x1F);
                if (start) {
                    start = false;
                    pkt->payload_[1] |= (1<<7);
                } else if (end) {
                    pkt->payload_[1] |= (1<<6);
                } else {
                    
                }
                
                memcpy(pkt->payload_ + 2, data + data_pos, payload_len);
                data_pos += payload_len;
                pkts.emplace_back(pkt);
            }

            if (data_pos != total_bytes) {
            }
        } else {//signal nalu
            auto pkt = std::make_shared<RtpPacket>();
            pkt->header_.pt = pt;
            pkt->header_.timestamp = pts;
            pkt->header_.seqnum = get_sequence_num();
            pkt->header_.ssrc = ssrc;
            pkt->payload_ = new char[nalu.size()];
            pkt->payload_len_ = nalu.size();
            memcpy(pkt->payload_, nalu.data(), nalu.size());
            if (last) {
                pkt->header_.marker = 1;
            } else {
                pkt->header_.marker = 0;
            }
            pkts.emplace_back(pkt);
        }
    }
    return pkts;
}

RtpPacker::~RtpPacker()
{

}


H265RtpPacker::H265RtpPacker()
{

}

H265RtpPacker::~H265RtpPacker()
{

}

std::vector<std::shared_ptr<RtpPacket>> H265RtpPacker::pack(const std::list<std::string_view> & nalus, uint8_t pt, int ssrc, int64_t pts) {
    std::vector<std::shared_ptr<RtpPacket>> pkts;
    int i = 0;
    for (auto & nalu : nalus) {
        i++;
        bool last = i == (int)nalus.size();
        if (nalu.size() > NAL_RTP_PACKET_SIZE - 3) {// FU_A
            // https://datatracker.ietf.org/doc/html/rfc7798#section-4.4.3
            int32_t data_pos = 0;
            bool start = true;
            bool end = false; 
            uint8_t nalu_type = (uint8_t)H265_TYPE(*nalu.data());
            char *data = (char*)nalu.data() + 2;
            int32_t total_bytes = nalu.size() - 2;
            
            while (data_pos < total_bytes) {
                auto pkt = std::make_shared<RtpPacket>();
                pkt->header_.pt = pt;
                pkt->header_.timestamp = pts;
                pkt->header_.marker = 0;
                pkt->header_.seqnum = get_sequence_num();
                pkt->header_.ssrc = ssrc;
                int32_t left_bytes = total_bytes - data_pos;
                int32_t payload_len = (left_bytes > NAL_RTP_PACKET_SIZE)?NAL_RTP_PACKET_SIZE:left_bytes;
                pkt->payload_ = new char[payload_len+3];
                pkt->payload_len_ = payload_len + 3;
                if ((data_pos + payload_len) >= total_bytes) {
                    end = true;
                    if (last) {
                        pkt->header_.marker = 1;
                    }
                }
                
                // char *p = pkt->payload_;
                pkt->payload_[0] = 49 << 1;
                pkt->payload_[1]  = 1;

                pkt->payload_[2] = nalu_type;
                if (start) {
                    start = false;
                    pkt->payload_[2] |= (1<<7);
                } else if (end) {
                    pkt->payload_[2] |= (1<<6);
                } else {
                    
                }
                
                memcpy(pkt->payload_ + 3, data + data_pos, payload_len);
                data_pos += payload_len;
                pkts.emplace_back(pkt);
            }
        } else {//signal nalu
            auto pkt = std::make_shared<RtpPacket>();
            pkt->header_.pt = pt;
            pkt->header_.timestamp = pts;
            pkt->header_.seqnum = get_sequence_num();
            pkt->header_.ssrc = ssrc;
            pkt->payload_ = new char[nalu.size()];
            pkt->payload_len_ = nalu.size();
            memcpy(pkt->payload_, nalu.data(), nalu.size());
            if (last) {
                pkt->header_.marker = 1;
            } else {
                pkt->header_.marker = 0;
            }
            pkts.emplace_back(pkt);
        }
    }
    return pkts;
}
