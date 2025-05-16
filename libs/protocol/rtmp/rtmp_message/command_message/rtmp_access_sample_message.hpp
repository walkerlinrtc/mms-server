#pragma once

#include "amf0/amf0_inc.hpp"
#include "rtmp_define.hpp"

namespace mms {
class RtmpAccessSampleMessage {
    friend class RtmpSession;
public:
    RtmpAccessSampleMessage(bool video = true, bool audio = true);
public:
    int32_t decode(std::shared_ptr<RtmpMessage> rtmp_msg);
    std::shared_ptr<RtmpMessage> encode() const;
    
private:
    Amf0String command_name_;
    Amf0Boolean audio_;
    Amf0Boolean video_;
};

};