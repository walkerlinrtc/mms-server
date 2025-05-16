#pragma once
#include <arpa/inet.h>
#include <stdint.h>
#include "protocol/rtp/rtp_packet.h"
namespace mms {
class RtpOverTcpPktHeader {
public:
    int32_t decode(uint8_t *data, size_t len) {
        uint8_t *data_start = data;
        if (len <= 0) {
            return 0;
        }

        if (data[0] != '$') {
            return -1;
        }
        data++;
        len--;

        if (len <= 0) {
            return 0;
        }
        channel_ = data[0];
        data++;
        len--;

        if (len < 2) {
            return 0;
        }
        pkt_len_ = ntohs((*(uint16_t*)data));
        len -= 2;
        data += 2;
        return data - data_start;
    }

    int32_t encode(uint8_t *data, size_t len) {
        uint8_t *data_start = data;
        if (len < 1) {
            return -1;
        }

        data[0] = '$';
        data++;
        len--;

        if (len < 1) {
            return -2;
        }
        data[0] = channel_;
        data++;
        len--;

        if (len < 2) {
            return -3;
        }
        *(uint16_t*)data = htons(pkt_len_);
        len -= 2;
        data += 2;
        return data - data_start;
    }

    uint8_t get_channel() const {
        return channel_;
    }

    uint16_t get_pkt_len() const {
        return pkt_len_;
    }

    void set_channel(uint8_t v) {
        channel_ = v;
    }

    void set_pkt_len(uint16_t v) {
        pkt_len_ = v;
    }
private:
    uint8_t channel_ = 0;
    uint16_t pkt_len_ = 0;
};
};