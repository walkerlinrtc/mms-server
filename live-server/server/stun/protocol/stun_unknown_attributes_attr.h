#pragma once
namespace mms {
    struct StunUnknownAttributesAttr : public StunMsgAttr
    {
        StunUnknownAttributesAttr() : StunMsgAttr(STUN_ATTR_UNKNOWN_ATTRIBUTES)
        {
        }

        uint16_t attr[4];
    };
};