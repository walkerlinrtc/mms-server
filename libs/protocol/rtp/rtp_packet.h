#pragma once
#include "rtp_header.h"
namespace mms {
    class RtpPacket
    {
    public:
        RtpPacket() {};
        virtual ~RtpPacket();

    public:
        virtual int32_t encode(uint8_t *data, size_t len);
        virtual int32_t decode_and_store(uint8_t *data, size_t len);
        virtual size_t size();
        uint16_t get_seq_num();
        uint32_t get_timestamp();

        std::string_view get_payload();
        uint8_t get_pt() {
            return header_.get_pt();
        }

        RtpHeader & get_header() {
            return header_;
        }
    public:
        RtpHeader header_;
        char *payload_ = nullptr;
        size_t payload_len_ = 0;
    };
};