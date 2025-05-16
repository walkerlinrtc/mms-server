#pragma once
#include <stdint.h>
#include <memory>
#include <string_view>

namespace mms {
enum PacketType {
    PACKET_UNKNOWN = -1,
    PACKET_RTMP = 0,
    PACKET_FLV = 1,
    PACKET_PES = 2,
    PACKET_TS = 3,
};

class Packet : public std::enable_shared_from_this<Packet> {
public:
    Packet();
    Packet(PacketType t, size_t bytes);
    virtual ~Packet();

    PacketType get_packet_type() const {
        return packet_type_;
    }

    void alloc_buf(size_t bytes);
    void inc_used_bytes(size_t s);
    std::string_view get_unuse_data() const;
    std::string_view get_using_data() const;
    std::string_view get_total_data() const;
protected:
    PacketType packet_type_ = PACKET_UNKNOWN;
    uint8_t * data_buf_ = nullptr;
    size_t data_bytes_ = 0;
    size_t using_bytes_ = 0;
};
};