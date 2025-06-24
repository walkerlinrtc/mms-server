#pragma once
#include "stun_define.hpp"
#include "stun_msg_attr.h"
namespace mms {
struct StunUnknownAttributesAttr : public StunMsgAttr
{
    StunUnknownAttributesAttr() : StunMsgAttr(STUN_ATTR_UNKNOWN_ATTRIBUTES)
    {
    }

    uint16_t attr[4];
};
};