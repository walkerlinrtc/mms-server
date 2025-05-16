#pragma once
#include <memory>
#include <string_view>
namespace mms {
class Chunk {
public:
    Chunk(size_t alloc_bytes);
    virtual ~Chunk();
public:
    size_t get_remaining_bytes();
    size_t get_used_bytes() const;
    size_t pos();
    std::string_view get_remaining_buf();
    std::string_view get_raw_buf();

    void skip(size_t bytes);
    uint8_t read_1bytes();
    uint16_t read_2bytes();
    uint32_t read_3bytes();
    uint32_t read_4bytes();
    uint64_t read_8bytes();
    std::string read_string(size_t len);
    void read_string(std::string_view v);
    void read_string(char *data, size_t len);

    void write_1bytes(uint8_t v);
    void write_2bytes(uint16_t v);
    void write_3bytes(uint32_t v);
    void write_4bytes(uint32_t v);
    void write_8bytes(uint64_t v);
    void write_string(const std::string & v);
    void write_string(const std::string_view & v);
    void write_string(const char *data, size_t len);
protected:
    std::unique_ptr<char[]> buf_;
    size_t allocated_bytes_;
    size_t used_bytes_ = 0;
};
};