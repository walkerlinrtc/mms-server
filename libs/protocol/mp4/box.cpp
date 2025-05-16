#include <string.h>
#include "box.h"
#include "base/net_buffer.h"
#include "spdlog/spdlog.h"
using namespace mms;

int64_t Box::size() {
    int64_t total_bytes = 8;
    if (size_ == 1) {
        total_bytes += 8;
    }

    if (type_ == BOX_TYPE_UUID) {
        total_bytes += 16;
    }

    return total_bytes;
}

int64_t Box::decoded_size() {
    if (size_ == 1) {
        return large_size_;
    } else {
        return size_;
    }
}

void Box::update_size() {
    size_ = size();
    if (size_ >= 0xffffffff) {
        large_size_ = size_;
        size_ = 1;
    }
}

int64_t Box::encode(NetBuffer & buf) {
    auto start = buf.pos();
    buf.write_4bytes(size_);
    buf.write_4bytes(type_);
    
    if (size_ == 1) {
        buf.write_8bytes(large_size_);
    }

    if (type_ == BOX_TYPE_UUID) {
        buf.write_string(std::string_view(user_type_, 16));
    }
    auto end = buf.pos();
    return end - start;
}

int64_t Box::decode(NetBuffer & buf) {
    auto start = buf.pos();
    size_ = buf.read_4bytes();
    type_ = buf.read_4bytes();

    if (size_ == 1) {
        large_size_ = buf.read_8bytes();
    }

    if (type_ == BOX_TYPE_UUID) {
        buf.read_string(std::string_view(user_type_, 16));
    }
    auto end = buf.pos();
    return end - start;
}