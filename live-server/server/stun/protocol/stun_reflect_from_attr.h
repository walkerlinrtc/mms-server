#pragma once
namespace mms {
    struct StunReflectFromAttr : public StunMsgAttr
    {
        StunReflectFromAttr() : StunMsgAttr(STUN_ATTR_REFLECTED_FROM)
        {
        }

        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };
};