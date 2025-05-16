#include "aac_encoder.hpp"
#include "faac.h"
#include "spdlog/spdlog.h"
#include <string.h>

using namespace mms;

#define MAX_PCM_BUF_BYTES 8192
AACEncoder::AACEncoder() {
    in_pcm_buf_ = std::make_unique<uint8_t[]>(MAX_PCM_BUF_BYTES);
    in_pcm_bytes_ = 0;
}

AACEncoder::~AACEncoder() {
    if (handle_) {
        faacEncClose(handle_);
        handle_ = nullptr;
    }
}

bool AACEncoder::init(int sample_rate, int channels) {
    handle_ = (faacEncHandle)faacEncOpen(sample_rate, channels, &input_samples_need_, &output_max_bytes_);
    if (!handle_) {
        return false;
    }

    sample_rate_ = sample_rate;
    channels_ = channels;

    auto conf = faacEncGetCurrentConfiguration(handle_);
    conf->inputFormat   = FAAC_INPUT_16BIT;
    conf->outputFormat  = RAW_STREAM;       ///no ADTS_STREAM;
    conf->useTns        = true;
    conf->useLfe        = false;
    conf->aacObjectType = LOW;
    conf->shortctl      = SHORTCTL_NORMAL;
    conf->quantqual     = 100;
    conf->bandWidth     = 0;
    conf->bitRate       = 0;
    faacEncSetConfiguration(handle_, conf);
    return true;
}

std::string AACEncoder::get_specific_configuration() {
    unsigned char *buffer      = nullptr;
    unsigned long len = 0;
    faacEncGetDecoderSpecificInfo(handle_, &buffer, &len);
    std::string config = std::string((char*)(buffer), len);
    if (buffer) {
        free(buffer);
    }
    return config;
}

int32_t AACEncoder::encode(uint8_t *data_in, int32_t in_bytes, uint8_t *data_out, int32_t out_bytes) {
    if (!handle_) {
        return -1;
    }

    if (in_pcm_bytes_ + in_bytes >= MAX_PCM_BUF_BYTES) {
        return -2;
    }

    memcpy(in_pcm_buf_.get() + in_pcm_bytes_, data_in, in_bytes);
    in_pcm_bytes_ += in_bytes;

    if (in_pcm_bytes_ >= (int32_t)input_samples_need_ * 2) {
        auto bytes = faacEncEncode(handle_,
                                (int32_t*)(in_pcm_buf_.get()),
                                channels_*1024,
                                data_out,
                                out_bytes);
        int32_t consumed = 1024 * channels_ * 2;
        in_pcm_bytes_ -= consumed;
        memmove(in_pcm_buf_.get(), in_pcm_buf_.get() + consumed, in_pcm_bytes_);
        return bytes;
    }

    return 0;
}