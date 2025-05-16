#pragma once
#include "rtcp_packet.h"
namespace mms {
struct ReceptionReportBlock {
    int32_t ssrc;
    uint8_t fraction_lost;
    int32_t cumulative_number_of_packets_lost;
    int32_t extended_highest_sequence_number_received;
    int32_t interarrival_jitter;
    int32_t last_SR;
    int32_t delay_since_last_SR;

    int32_t decode(uint8_t *data, int32_t len);
    int32_t encode(uint8_t *data, int32_t len);
};

class RtcpSr : public RtcpPacket {
public:
    RtcpSr();
    virtual ~RtcpSr();

    int32_t decode(uint8_t *data, int32_t len);
    int32_t encode(uint8_t *data, int32_t len);
public:
    uint32_t ntp_timestamp_sec_;
    uint32_t ntp_timestamp_psec_;
    uint32_t rtp_time_;
    int32_t sender_packet_count_;
    int32_t sender_octet_count_;
    std::vector<ReceptionReportBlock> reception_report_blocks;
};
};