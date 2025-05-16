#include "spdlog/spdlog.h"
#include "bit_stream.hpp"
using namespace mms;

BitStream::BitStream(const std::string_view & stream) : stream_(stream) {

}

bool BitStream::read_one_bit(uint8_t & b) {
    uint8_t v;
    try {
        v = (uint8_t)stream_.at(bit_pos_/8);
    } catch (std::exception & e) {
        return false;
    }
    
    if (v & (0x80 >> (bit_pos_ % 8))) {
        b = 1;
    } else {
        b = 0;
    }
    bit_pos_++;
    return true;
}

bool BitStream::read_u(size_t count, uint8_t & v) {
    const char * buf = stream_.data();
    v = 0;
    for (size_t i = 0; i < count; i++)
    {
        v <<= 1;
        if (buf[bit_pos_ / 8] & (0x80 >> (bit_pos_ % 8)))
        {
            v += 1;
        }
        bit_pos_++;
    }
    return true;
}

bool BitStream::read_u(size_t count, uint16_t & v) {
    // v = 0;
    // for (size_t i = 0; i < count; i++) {
    //     uint8_t bit;
    //     if (!read_one_bit(bit)) {
    //         return false;
    //     }
    //     v = (v << 1) | bit;
    // }

    const char * buf = stream_.data();
    v = 0;
    for (size_t i = 0; i < count; i++)
    {
        v <<= 1;
        if (buf[bit_pos_ / 8] & (0x80 >> (bit_pos_ % 8)))
        {
            v += 1;
        }
        bit_pos_++;
    }
    return true;
}

bool BitStream::read_u(size_t count, uint64_t & v) {
    const char * buf = stream_.data();
    v = 0;
    for (size_t i=0; i < count; i++)
    {
        v <<= 1;
        if (buf[bit_pos_ / 8] & (0x80 >> (bit_pos_ % 8)))
        {
            v += 1;
        }
        bit_pos_++;
    }
    return true;
}

bool BitStream::write_one_bit(uint8_t v) {
    char *byte = (char*)stream_.data() + bit_pos_/8;
    if (v == 1) {
        *byte = *byte | (v << (7 - bit_pos_%8));
    } else {
        *byte = *byte & ~(v << (7 - bit_pos_%8));
    }
    bit_pos_++;
    return true;
}

bool BitStream::write_bits(size_t count, uint64_t v) {
    for (size_t i = 0; i < count; i++) {
        uint8_t b = (v >> (count - i - 1)) & 0x01;
        if (!write_one_bit(b)) {
            return false;
        }
        // v = v >> 1;
    }
    return true;
}

/*
    Parsing process for Exp-Golomb codes
    This process is invoked when the descriptor of a syntax element in the syntax tables in clause 7.3 is equal to ue(v), me(v), 
    se(v), or te(v). For syntax elements in clauses 7.3.4 and 7.3.5, this process is invoked only when 
    entropy_coding_mode_flag is equal to 0.
    Inputs to this process are bits from the RBSP.
    Outputs of this process are syntax element values.
    Syntax elements coded as ue(v), me(v), or se(v) are Exp-Golomb-coded. Syntax elements coded as te(v) are truncated ExpGolomb-coded. The parsing process for these syntax elements begins with reading the bits starting at the current location 
    in the bitstream up to and including the first non-zero bit, and counting the number of leading bits that are equal to 0. This 
    process is specified as follows:
    leadingZeroBits = −1
    for( b = 0; !b; leadingZeroBits++ ) (9-1)
    b = read_u( 1 )
    The variable codeNum is then assigned as follows:
    codeNum = 2^leadingZeroBits − 1 + read_u( leadingZeroBits ) (9-2)
*/
bool BitStream::read_exp_golomb_ue(uint64_t & v) {
    int leadingZeroBits = -1;
    uint8_t b = 0;
    for (b = 0; !b && leadingZeroBits < 64; leadingZeroBits++) {
        if (!read_u(1, b)) {
            return false;
        }
    }
    uint64_t l;
    if (!read_u(leadingZeroBits, l)) {
        return false;
    }
    v = (1 << leadingZeroBits) - 1 + l;
    return true;
}

bool BitStream::read_exp_golomb_se(int64_t & v) {
    uint64_t ue;
    if (!read_exp_golomb_ue(ue)) {
        return false;
    }
    bool positive = ue & 0x01;
    v = (ue + 1) >> 1;
    if (!positive) {
        v = -v;
    }
    return true;
}