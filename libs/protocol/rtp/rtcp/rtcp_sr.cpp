#include <arpa/inet.h>

#include "rtcp_sr.h"
using namespace mms;
int32_t ReceptionReportBlock::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 4) {
        return 0;
    }
    ssrc = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    fraction_lost = data[0];
    uint8_t *p = (uint8_t*)&cumulative_number_of_packets_lost;
    cumulative_number_of_packets_lost = 0;
    p[0] = data[3];
    p[1] = data[2];
    p[2] = data[1];
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    extended_highest_sequence_number_received = ntohl(*(uint32_t *)data);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    interarrival_jitter = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    last_SR = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    delay_since_last_SR = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;
    return data - data_start;
}

int32_t ReceptionReportBlock::encode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(ssrc);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    data[0] = fraction_lost;
    uint8_t *p = (uint8_t*)&cumulative_number_of_packets_lost;
    cumulative_number_of_packets_lost = 0;
    data[1] = p[3];
    data[2] = p[2];
    data[3] = p[1];
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(extended_highest_sequence_number_received);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(interarrival_jitter);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(last_SR);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(delay_since_last_SR);
    data += 4;
    len -= 4;
    return data - data_start;
}

RtcpSr::RtcpSr() {

}

RtcpSr::~RtcpSr() {

}

int32_t RtcpSr::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    int32_t consumed = header_.decode(data, len);
    if (consumed <= 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;
    if (len < 8) {
        return 0;
    }

    ntp_timestamp_sec_ = ntohl(*(uint32_t*)data);
    data += 4;
    ntp_timestamp_psec_ = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 8;

    if (len < 4) {
        return 0;
    }
    rtp_time_ = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    sender_packet_count_ = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;
    
    if (len < 4) {
        return 0;
    }
    sender_octet_count_ = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    for (uint8_t i = 0; i < header_.rc; i++) {
        ReceptionReportBlock reception_report_block;
        consumed = reception_report_block.decode(data, len);
        if (consumed <= 0) {
            return 0;
        }
        data += consumed;
        len -= consumed;
        reception_report_blocks.emplace_back(reception_report_block);
    }
    return data - data_start;
}

int32_t RtcpSr::encode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    int32_t consumed = header_.encode(data, len);
    if (consumed <= 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;
    if (len < 8) {
        return 0;
    }

    *(uint32_t*)data = htonl(ntp_timestamp_sec_);
    data += 4;
    *(uint32_t*)data = htonl(ntp_timestamp_psec_);
    data += 4;
    len -= 8;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(rtp_time_);
    data += 4;
    len -= 4;

    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(sender_packet_count_);
    data += 4;
    len -= 4;
    
    if (len < 4) {
        return 0;
    }
    *(uint32_t*)data = htonl(sender_octet_count_);
    data += 4;
    len -= 4;
    
    for (auto & r : reception_report_blocks) {
        consumed = r.encode(data, len);
        if (consumed <= 0) {
            return 0;
        }
        data += consumed;
        len -= consumed;
    }
    return data - data_start;
}