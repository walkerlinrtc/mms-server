#pragma once
#include <stdint.h>
#include "rtmp_define.hpp"

namespace mms {
class RtmpWindowAckSizeMessage {
public:
    RtmpWindowAckSizeMessage(uint32_t size) : ack_window_size_(size) {

    }

    uint32_t size() {
        return 0;
    }

    int32_t decode(std::shared_ptr<RtmpMessage> rtmp_msg);
    std::shared_ptr<RtmpMessage> encode() const;
private:
    uint32_t ack_window_size_ = 0;
};
};