#pragma once
#include <stdint.h>
#include <string>
#include <string_view>
namespace mms {
class AACDecoder {
public:
    AACDecoder();
    virtual ~AACDecoder();
    bool init(const std::string & decoder_config);
    unsigned long get_sample_rate();
    unsigned char get_channels();
    std::string_view decode(uint8_t *data_in, int32_t in_len);
private:
    void *handle_ = nullptr;
    unsigned long sample_rate_ = 0;
    unsigned char channels_ = 0;
};
};