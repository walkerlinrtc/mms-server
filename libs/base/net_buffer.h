#pragma once
#include <string_view>
#include <string>
#include <stdint.h>
namespace mms {
class NetBuffer {
public:
    NetBuffer(std::string_view buf);
    virtual ~NetBuffer();
public:
    void skip(size_t bytes);
    uint8_t read_1bytes();
    uint16_t read_2bytes();
    uint32_t read_3bytes();
    uint32_t read_4bytes();
    uint64_t read_8bytes();
    std::string read_string(size_t len);
    void read_string(std::string_view v);

    void write_1bytes(uint8_t v);
    void write_2bytes(uint16_t v);
    void write_3bytes(uint32_t v);
    void write_4bytes(uint32_t v);
    void write_8bytes(uint64_t v);
    void write_string(const std::string & v);
    void write_string(const std::string_view & v);

    std::string_view get_curr_buf() {
        return buf_;
    }
    uint32_t pos();
    uint32_t left_bytes();
protected:
    std::string_view buf_;
    std::string_view initial_buf_;
};
};