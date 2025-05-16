#pragma once
#include <memory>
#include <fstream>

namespace mms {
class AACEncoder {
public:
    AACEncoder();
    virtual ~AACEncoder();
    bool init(int sample_rate, int channels);
    int32_t encode(uint8_t *data_in, int32_t in_len, uint8_t *data_out, int32_t out_len);

    std::string get_specific_configuration();
private:
    void *handle_ = nullptr;
    int32_t sample_rate_;
    int32_t channels_;
    unsigned long input_samples_need_ = 1024;
    unsigned long output_max_bytes_ = 1024*2*2;
    std::unique_ptr<uint8_t[]> in_pcm_buf_;
    int32_t in_pcm_bytes_ = 0;
};
};