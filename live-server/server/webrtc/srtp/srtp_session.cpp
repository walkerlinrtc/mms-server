#include "spdlog/spdlog.h"
#include <string.h>
#include "srtp_session.h"
using namespace mms;

bool SRTPSession::init(SRTPProtectionProfile profile, const std::string &recv_key, const std::string &send_key)
{
    memset(&srtp_policy_, 0, sizeof(srtp_policy_t));
    switch (profile)
    {
    case SRTP_AES128_CM_HMAC_SHA1_80:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&srtp_policy_.rtp);
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&srtp_policy_.rtcp);
        break;
    default:
        break;
    }

    if ((int)recv_key.size() != srtp_policy_.rtp.cipher_key_len) {
        spdlog::error("recv_key was not match, len:{}, expected:{}", recv_key.size(), srtp_policy_.rtp.cipher_key_len);
    }
    
    if ((int)send_key.size() != srtp_policy_.rtp.cipher_key_len) {
        spdlog::error("send_key was not match, len:{}, expected:{}", send_key.size(), srtp_policy_.rtp.cipher_key_len);
    };


    srtp_policy_.ssrc.value = 0;
    srtp_policy_.allow_repeat_tx = 1;
    srtp_policy_.window_size = 4096;
    srtp_policy_.next = NULL;

    srtp_policy_.ssrc.type = ssrc_any_inbound;
    srtp_policy_.key = (unsigned char *)(recv_key.c_str());
    auto err = srtp_create(&recv_ctx_, &srtp_policy_);
    if (err != srtp_err_status_ok)
    {
        return false;
    }

    srtp_policy_.ssrc.type = ssrc_any_outbound;
    srtp_policy_.key = (unsigned char *)(send_key.c_str());
    err = srtp_create(&send_ctx_, &srtp_policy_);
    if (err != srtp_err_status_ok)
    {
        return false;
    }

    return true;
}

int32_t SRTPSession::unprotect_srtp(uint8_t *data, size_t len)
{
    if (len >= SRTP_MAX_BUFFER_SIZE)
    {
        return -1;
    }

    int out_len = len;
    auto err = srtp_unprotect(recv_ctx_, data, (int*)&out_len);
    if (err != srtp_err_status_ok)
    {
        return -2;
    }
    return out_len;
}

int32_t SRTPSession::unprotect_srtcp(uint8_t *data, size_t len)
{
    if (len >= SRTP_MAX_BUFFER_SIZE)
    {
        return -1;
    }

    int out_len = len;
    auto err = srtp_unprotect_rtcp(recv_ctx_, data, (int*)&out_len);
    if (err != srtp_err_status_ok)
    {
        return -2;
    }
    return out_len;
}

int32_t SRTPSession::protect_srtp(uint8_t *in, size_t in_len, uint8_t *out, size_t & out_len)
{
    if (!send_ctx_) {
        return 0;
    }

    if (in_len + SRTP_MAX_TRAILER_LEN > SRTP_MAX_BUFFER_SIZE) {
        return -1;
    }

    int inout_bytes = in_len;
    memcpy(out, in, in_len);
    auto err = srtp_protect(send_ctx_, out, &inout_bytes);
    if (err != srtp_err_status_ok) {
        spdlog::error("srtp_protect error, err:{}", (int)err);
        return -2;
    }
    out_len = inout_bytes;
    return inout_bytes;
}

int32_t SRTPSession::protect_srtcp(uint8_t *in, size_t in_len, uint8_t *out, size_t & out_len)
{
    if (!send_ctx_) {
        return 0;
    }
    
    if (in_len + SRTP_MAX_TRAILER_LEN > SRTP_MAX_BUFFER_SIZE) {
        return -1;
    }

    int inout_bytes = in_len;
    memcpy(out, in, in_len);
    auto err = srtp_protect_rtcp(send_ctx_, out, &inout_bytes);
    if (err != srtp_err_status_ok) {
        return -2;
    }
    out_len = inout_bytes;
    return inout_bytes;
}
