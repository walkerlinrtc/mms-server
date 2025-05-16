#include "net_buffer.h"
#include <cassert>
#include <arpa/inet.h>
#include <string.h>

using namespace mms;

NetBuffer::NetBuffer(std::string_view buf) : buf_(buf), initial_buf_(buf) {

}

NetBuffer::~NetBuffer() {

}

void NetBuffer::skip(size_t bytes) {
    assert(buf_.size() >= bytes);
    buf_.remove_prefix(bytes);
}

uint8_t NetBuffer::read_1bytes() {
    assert(buf_.size() >= 1);
    uint8_t v  = *(uint8_t*)buf_.data();
    buf_.remove_prefix(1);
    return v;
}

uint16_t NetBuffer::read_2bytes() {
    assert(buf_.size() >= 2);
    uint16_t v = htonl(*(uint16_t*)buf_.data());
    buf_.remove_prefix(2);
    return v;
}

uint32_t NetBuffer::read_3bytes() {
    assert(buf_.size() >= 3);
    uint32_t v = 0;
    char *p = (char*)&v;
    char *q = (char*)buf_.data();
    p[0] = q[2];
    p[1] = q[1];
    p[2] = q[0];
    buf_.remove_prefix(3);
    return v;
}

uint32_t NetBuffer::read_4bytes() {
    assert(buf_.size() >= 4);
    uint32_t v = htonl(*(uint32_t*)buf_.data());
    buf_.remove_prefix(4);
    return v;
}

uint64_t NetBuffer::read_8bytes() {
    assert(buf_.size() >= 8);
    uint64_t v = be64toh(*(uint64_t*)buf_.data());
    buf_.remove_prefix(8);
    return v;
}

std::string NetBuffer::read_string(size_t len) {
    assert(buf_.size() >= len);
    auto v = std::string(buf_.data(), len);
    buf_.remove_prefix(len);
    return v;
}

void NetBuffer::read_string(std::string_view v) {
    assert(buf_.size() >= v.size());
    memcpy((char*)v.data(), buf_.data(), v.size());
    buf_.remove_prefix(v.size());
}

void NetBuffer::write_1bytes(uint8_t v) {
    assert(buf_.size() >= 1);
    *(uint8_t*)buf_.data() = v;
    buf_.remove_prefix(1);
}

void NetBuffer::write_2bytes(uint16_t v) {
    assert(buf_.size() >= 2);
    *(uint16_t*)buf_.data() = htons(v);
    buf_.remove_prefix(2);
}

void NetBuffer::write_3bytes(uint32_t v) {
    assert(buf_.size() >= 3);
    char *p = (char*)&v;
    char *q = (char*)buf_.data();
    q[0] = p[2];
    q[1] = p[1];
    q[2] = p[0];
    buf_.remove_prefix(3);
}

void NetBuffer::write_4bytes(uint32_t v) {
    assert(buf_.size() >= 4);
    *(uint32_t*)buf_.data() = htonl(v);
    buf_.remove_prefix(4);
}

void NetBuffer::write_8bytes(uint64_t v) {
    assert(buf_.size() >= 8);
    *(uint64_t*)buf_.data() = htobe64(v);
    buf_.remove_prefix(8);
}

void NetBuffer::write_string(const std::string & v) {
    assert(buf_.size() >= v.size());
    memcpy((char*)buf_.data(), v.data(), v.size());
    buf_.remove_prefix(v.size());
}

void NetBuffer::write_string(const std::string_view & v) {
    assert(buf_.size() >= v.size());
    memcpy((char*)buf_.data(), v.data(), v.size());
    buf_.remove_prefix(v.size());
}

uint32_t NetBuffer::pos() {
    return buf_.data() - initial_buf_.data();
}

uint32_t NetBuffer::left_bytes() {
    return buf_.size();
}