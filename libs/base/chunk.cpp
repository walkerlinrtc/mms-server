#include <cassert>
#include <string.h>
#include <arpa/inet.h>

#include "chunk.h"
using namespace mms;
Chunk::Chunk(size_t alloc_bytes) : allocated_bytes_(alloc_bytes) {
    buf_ = std::make_unique<char[]>(alloc_bytes);
}

Chunk::~Chunk() {

}

size_t Chunk::get_remaining_bytes() {
    return allocated_bytes_ - used_bytes_;
}

size_t Chunk::get_used_bytes() const {
    return used_bytes_;
}

std::string_view Chunk::get_remaining_buf() {
    return std::string_view((char*)buf_.get() + used_bytes_, get_remaining_bytes());
}

std::string_view Chunk::get_raw_buf() {
    return std::string_view((char*)buf_.get(), allocated_bytes_);
}

size_t Chunk::pos() {
    return used_bytes_;
}

void Chunk::skip(size_t bytes) {
    used_bytes_ += bytes;
}

uint8_t Chunk::read_1bytes() {
    uint8_t v  = *(uint8_t*)buf_.get();
    used_bytes_ += 1;
    return v;
}

uint16_t Chunk::read_2bytes() {
    uint16_t v = htonl(*(uint16_t*)buf_.get());
    used_bytes_ += 2;
    return v;
}

uint32_t Chunk::read_3bytes() {
    uint32_t v = 0;
    char *p = (char*)&v;
    char *q = (char*)buf_.get();
    p[0] = q[2];
    p[1] = q[1];
    p[2] = q[0];
    used_bytes_ += 3;
    return v;
}

uint32_t Chunk::read_4bytes() {
    uint32_t v = htonl(*(uint32_t*)buf_.get());
    used_bytes_ += 4;
    return v;
}

uint64_t Chunk::read_8bytes() {
    uint64_t v = be64toh(*(uint64_t*)buf_.get());
    used_bytes_ += 8;
    return v;
}

std::string Chunk::read_string(size_t len) {
    auto v = std::string(buf_.get(), len);
    used_bytes_ += len;
    return v;
}

void Chunk::read_string(std::string_view v) {
    memcpy((char*)v.data(), buf_.get(), v.size());
    used_bytes_ += v.size();
}

void Chunk::read_string(char *data, size_t len) {
    memcpy(data, buf_.get(), len);
    used_bytes_ += len;
}

void Chunk::write_1bytes(uint8_t v) {
    *(uint8_t*)buf_.get() = v;
    used_bytes_ += 1;
}

void Chunk::write_2bytes(uint16_t v) {
    *(uint16_t*)buf_.get() = v;
    used_bytes_ += 2;
}

void Chunk::write_3bytes(uint32_t v) {
    char *p = (char*)&v;
    char *q = (char*)buf_.get();
    q[0] = p[2];
    q[1] = p[1];
    q[2] = p[0];
    used_bytes_ += 3;
}

void Chunk::write_4bytes(uint32_t v) {
    *(uint32_t*)buf_.get() = v;
    used_bytes_ += 4;
}

void Chunk::write_8bytes(uint64_t v) {
    *(uint64_t*)buf_.get() = htobe64(v);
    used_bytes_ += 8;
}

void Chunk::write_string(const std::string & v) {
    memcpy((char*)buf_.get(), v.data(), v.size());
    used_bytes_ += v.size();
}

void Chunk::write_string(const std::string_view & v) {
    memcpy((char*)buf_.get(), v.data(), v.size());
    used_bytes_ += v.size();
}

void Chunk::write_string(const char *data, size_t len) {
    memcpy((char*)buf_.get(), data, len);
    used_bytes_ += len;
}
