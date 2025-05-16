#pragma once
#include "rtmp_define.hpp"

namespace mms {
class RtmpUserCtrlMessage {
public:
    RtmpUserCtrlMessage(uint16_t type) {
        event_type_ = type;
    }
protected:
    uint16_t event_type_;
};

};