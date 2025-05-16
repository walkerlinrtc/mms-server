#pragma once
#include <string_view>
#include <stdint.h>
#include <exception>

namespace mms {
class BitStream {
public:
    BitStream(const std::string_view & stream);
    bool read_one_bit(uint8_t & b);
    bool read_u(size_t count, uint8_t & v);
    bool read_u(size_t count, uint16_t & v);
    bool read_u(size_t count, uint64_t & v);

    bool write_one_bit(uint8_t v);
    bool write_bits(size_t count, uint64_t v);
    bool read_exp_golomb_ue(uint64_t & v);
    bool read_exp_golomb_se(int64_t & v);
public:
    std::string_view stream_;
    size_t bit_pos_ = 0;
};
};