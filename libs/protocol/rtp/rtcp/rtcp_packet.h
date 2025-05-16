#pragma once
#include "rtcp_header.h"
namespace mms {
    class RtcpPacket
    {
    public:
        RtcpPacket() {};
        virtual ~RtcpPacket();

    public:
        virtual int32_t encode(uint8_t *data, int32_t len);
        virtual int32_t decode_and_store(uint8_t *data, int32_t len);
        virtual size_t size();

        std::string_view get_payload();
        uint8_t get_pt() {
            return header_.get_pt();
        }

        RtcpHeader & get_header() {
            return header_;
        }
    public:
        RtcpHeader header_;
        char *payload_ = nullptr;
        size_t payload_len_ = 0;
    };
};