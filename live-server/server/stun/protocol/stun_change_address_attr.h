#pragma once
namespace mms
{
    struct StunChangeAddressAttr : public StunMsgAttr
    {
        StunChangeAddressAttr() : StunMsgAttr(STUN_ATTR_CHANGED_ADDRESS)
        {
        }

        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };
};