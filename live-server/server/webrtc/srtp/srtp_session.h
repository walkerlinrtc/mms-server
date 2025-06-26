#pragma once
#include <mutex>
#include "srtp2/srtp.h"
#include "../dtls/define.h"
#include <string>

#define SRTP_MAX_BUFFER_SIZE 65535
namespace mms
{
    class SRTPSession
    {
    public:
        bool init(SRTPProtectionProfile policy, const std::string &recv_key, const std::string &send_key);
        int32_t unprotect_srtp(uint8_t *data, size_t len);
        int32_t unprotect_srtcp(uint8_t *data, size_t len);
        int32_t protect_srtp(uint8_t *in, size_t in_len, uint8_t *out, size_t & out_len);
        int32_t protect_srtcp(uint8_t *in, size_t in_len, uint8_t *out, size_t & out_len);
        int32_t get_srtp_overhead() const 
        {
            return srtp_policy_.rtp.auth_tag_len;
        }

        int32_t get_srtcp_overhead() const
        {
            return srtp_policy_.rtcp.auth_tag_len;
        }
    private:
        srtp_t send_ctx_ = nullptr;
        srtp_t recv_ctx_ = nullptr;
        srtp_policy_t srtp_policy_;
    };
};