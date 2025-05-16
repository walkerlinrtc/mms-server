#pragma once
#include <string>
#include <string_view>

#include "full_box.h"
namespace mms {
class NetBuffer;
#define HANDLER_TYPE(c1, c2, c3, c4)       \
(((static_cast<uint32_t>(c1))<<24) |  \
    ((static_cast<uint32_t>(c2))<<16) |  \
    ((static_cast<uint32_t>(c3))<< 8) |  \
    ((static_cast<uint32_t>(c4))))

// 8.4.3 Handler Reference Box (hdlr)
// ISO_IEC_14496-12-base-format-2012.pdf, page 37
// This box within a Media Box declares the process by which the media-data in the track is presented, and thus,
// The nature of the media in a track. For example, a video track would be handled by a video handler.
class HdlrBox : public FullBox {
public:
    typedef uint32_t HandlerType;
public:
    HdlrBox();
    virtual ~HdlrBox();
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
public:
    bool is_video();
    bool is_audio();
public:
    uint32_t pre_defined_ = 0; 
    HandlerType handler_type_; 
    uint32_t reserved_[3] = {0}; 
    std::string name_;
};

const HdlrBox::HandlerType HANDLER_TYPE_SOUN = HANDLER_TYPE('s', 'o', 'u', 'n');
const HdlrBox::HandlerType HANDLER_TYPE_VIDE = HANDLER_TYPE('v', 'i', 'd', 'e');
const HdlrBox::HandlerType HANDLER_TYPE_HINT = HANDLER_TYPE('h', 'i', 'n', 't');

};