#include "opus_codec.hpp"
#include "protocol/sdp/media-level/media_sdp.hpp"
using namespace mms;

std::shared_ptr<OpusDecoder> OpusCodec::create_decoder(int32_t fs, int32_t channels) {
    int error;
    decoder_ = std::shared_ptr<OpusDecoder>(opus_decoder_create(fs, channels, &error), [](OpusDecoder *decoder) {
        opus_decoder_destroy(decoder);
    });

    if (error) {
        decoder_ = nullptr;
        return nullptr;
    }
    return decoder_;
}

std::shared_ptr<OpusCodec> OpusCodec::create_from_sdp(const MediaSdp & media_sdp, const Payload & payload) {
    ((void)media_sdp);
    const auto & params = payload.get_encoding_params();
    if (params.size() < 1) {
        return nullptr;
    }
    try {
        std::shared_ptr<OpusCodec> codec = std::make_shared<OpusCodec>();
        codec->fs_ = (int32_t)payload.get_clock_rate();
        codec->channels_ = std::atoi(params[0].c_str());
        codec->sdp_payload_ = std::make_shared<Payload>();
        *(codec->sdp_payload_) = payload;
        codec->ready_ = true;
        return codec;
    } catch (std::exception & exp) {
        return nullptr;
    }
}

std::shared_ptr<Payload> OpusCodec::get_payload() {
    return sdp_payload_;
}

