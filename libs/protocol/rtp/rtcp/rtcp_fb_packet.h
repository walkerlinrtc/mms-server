#pragma once
#include "rtcp_fb_header.h"
namespace mms {
    class RtcpFbPacket
    {
    public:
        RtcpFbPacket() {};
        virtual ~RtcpFbPacket();

    public:
        virtual int32_t encode(uint8_t *data, size_t len);
        virtual int32_t decode_and_store(uint8_t *data, size_t len);
        virtual size_t size();

        std::string_view get_payload();
        uint8_t get_pt() {
            return header_.get_pt();
        }

        RtcpFbHeader & get_header() {
            return header_;
        }

        
        void set_ssrc(uint32_t v) {
            header_.sender_ssrc = v;
        }

        void set_media_source_ssrc(uint32_t v) {
            header_.media_source_ssrc = v;
        }

    public:
        RtcpFbHeader header_;
        char *payload_ = nullptr;
        size_t payload_len_ = 0;
    };
};