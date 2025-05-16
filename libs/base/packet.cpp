#include <string.h>

#include "packet.h"
using namespace mms;

Packet::Packet() {

}

Packet::Packet(PacketType t, size_t bytes) {
    packet_type_ = t;
    data_buf_ = (uint8_t*)malloc(bytes);
    data_bytes_ = bytes;
    using_bytes_ = 0;
}

Packet::~Packet() {
    if (data_buf_) {
        free(data_buf_);
        data_buf_ = nullptr;
        data_bytes_ = 0;
        using_bytes_ = 0;
    }
}

void Packet::alloc_buf(size_t bytes) {
    if (data_buf_) {
        free(data_buf_);
        data_buf_ = nullptr;
        data_bytes_ = 0;
        using_bytes_ = 0;
    }

    data_buf_ = (uint8_t*)malloc(bytes);
    data_bytes_ = bytes;
    using_bytes_ = 0;
}

std::string_view Packet::get_unuse_data() const {
    return std::string_view((char*)data_buf_ + using_bytes_, data_bytes_ - using_bytes_);
}

std::string_view Packet::get_using_data() const {
    return std::string_view((char*)data_buf_, using_bytes_);
}

std::string_view Packet::get_total_data() const {
    return std::string_view((char*)data_buf_, data_bytes_);
}

void Packet::inc_used_bytes(size_t s) {
    using_bytes_ += s;
}