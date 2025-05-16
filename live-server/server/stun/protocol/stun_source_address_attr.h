#pragma once
namespace mms {
    struct StunSourceAddressAttr : public StunMsgAttr
    {
        StunSourceAddressAttr() : StunMsgAttr(STUN_ATTR_SOURCE_ADDRESS)
        {
        }

        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };

};