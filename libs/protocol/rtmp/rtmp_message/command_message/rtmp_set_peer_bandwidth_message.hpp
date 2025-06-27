#pragma once
#include <stdint.h>
#include <memory>

namespace mms {
class RtmpMessage;
class RtmpSetPeerBandwidthMessage {
public:
    // 0 - Hard: The peer SHOULD limit its output bandwidth to the
    // indicated window size.
    // 1 - Soft: The peer SHOULD limit its output bandwidth to the the
    // window indicated in this message or the limit already in effect,
    // whichever is smaller.
    // 2 - Dynamic: If the previous Limit Type was Hard, treat this message
    // as though it was marked Hard, otherwise ignore this message.
    #define LIMIE_TYPE_HARD     0
    #define LIMIT_TYPE_SOFT     1
    #define LIMIT_TYPE_DYNAMIC  2
public:
    RtmpSetPeerBandwidthMessage(uint32_t size, uint8_t limit_type) : ack_window_size_(size), limit_type_(limit_type) {

    }

    uint32_t size() {
        return 0;
    }

    int32_t decode(std::shared_ptr<RtmpMessage> rtmp_msg);

    std::shared_ptr<RtmpMessage> encode() const;
private:
    uint32_t ack_window_size_ = 0;
    uint8_t limit_type_;
};
};