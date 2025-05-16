#include "spdlog/spdlog.h"
#include "mpeg4_aac.hpp"
#include "base/utils/bit_stream.hpp"
using namespace mms;
const int mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

int32_t AudioSpecificConfig::parse(uint8_t *data, size_t len) {
    BitStream bit_stream(std::string_view((char*)data, len));
    if (len < 1) {
        return -1;
    }

    uint64_t v = 0;
    uint16_t consumed_bits = 0;
    if (!bit_stream.read_u(5, v)) {
        return -2;
    }
    consumed_bits += 5;

    audio_object_type = (AudioObjectType)(v & 0x1f);
    if (audio_object_type == AOT_ESCAPE) {
        if (!bit_stream.read_u(6, v)) {
            return -3;
        }
        consumed_bits += 6;
    }

    if (!bit_stream.read_u(4, v)) {
        return -4;
    }
    consumed_bits += 4;
    frequency_index = v & 0xf;
    if (frequency_index == 0x0f) {
        if (!bit_stream.read_u(24, v)) {
            return -5;
        }
        consumed_bits += 24;
        sampling_frequency = v & 0xfff;
    } else {
        sampling_frequency = mpeg4audio_sample_rates[frequency_index];
    }

    if (!bit_stream.read_u(4, v)) {
        return -6;
    }
    consumed_bits += 4;
    channel_configuration = v & 0x0f;
    return (consumed_bits + 7)/8;
}

int32_t AudioSpecificConfig::encode(uint8_t *data, size_t len) {
    BitStream bit_stream(std::string_view((char*)data, len));
    if (len < 1) {
        return -1;
    }

    uint64_t v = audio_object_type;
    uint16_t consumed_bits = 0;
    if (!bit_stream.write_bits(5, v)) {
        return -2;
    }
    consumed_bits += 5;

    if (audio_object_type == AOT_ESCAPE) {
        if (!bit_stream.write_bits(6, 0)) {
            return -3;
        }
        consumed_bits += 6;
    }

    if (!bit_stream.write_bits(4, frequency_index)) {
        return -4;
    }
    consumed_bits += 4;
    if (frequency_index == 0x0f) {
        if (!bit_stream.write_bits(24, sampling_frequency)) {
            return -5;
        }
        consumed_bits += 24;
    } 

    if (!bit_stream.write_bits(4, channel_configuration)) {
        return -6;
    }
    consumed_bits += 4;
    return (consumed_bits + 7)/8;
}

int32_t AudioSpecificConfig::size() {
    int32_t consumed_bits = 0;
    consumed_bits = 5;
    if (audio_object_type == AOT_ESCAPE) {
        consumed_bits += 6;
    }
    consumed_bits += 4;
    if (frequency_index == 0x0f) {
        consumed_bits += 24;
    }
    consumed_bits += 4;
    return (consumed_bits + 7)/8;
}