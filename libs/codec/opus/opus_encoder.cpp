#include "opus_encoder.hpp"
#include "opus/opus.h"
using namespace mms;

MOpusEncoder::MOpusEncoder() {
    
}

MOpusEncoder::~MOpusEncoder() {
    if (handle_) {
        opus_encoder_destroy((OpusEncoder*)handle_);
        handle_ = nullptr;
    }
}

bool MOpusEncoder::init(int32_t sample_rate, int32_t channels) {
    if (handle_) {
        return true;
    }

    int error = 0;
    handle_ = (void*)opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_AUDIO, &error);
    if (!handle_ || error != OPUS_OK) {
        return false;
    }

    int ret = opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_BITRATE(32000));
    if (ret != OPUS_OK) {
        opus_encoder_destroy((OpusEncoder*)handle_);
        handle_ = nullptr;
        return false;
    }
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_VBR(0));//0:CBR, 1:VBR
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_VBR_CONSTRAINT(true));
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_BITRATE(96000));
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_COMPLEXITY(8));//8    0~10
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_LSB_DEPTH(16));//每个采样16个bit，2个byte
    // opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_DTX(0));
    opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_PACKET_LOSS_PERC(0));
    opus_encoder_ctl((OpusEncoder*)handle_, OPUS_SET_INBAND_FEC(1));
    
    return true;
}

int32_t MOpusEncoder::encode(uint8_t *data_in, int32_t in_len, uint8_t *data_out, int32_t out_len) {
    auto len = opus_encode((OpusEncoder*)handle_, (const opus_int16*)data_in, in_len, data_out, out_len);
    return len;
}

