#pragma once
#include "stun_msg_attr.h"
#include "stun_define.hpp"
namespace mms {
    struct StunSourceAddressAttr : public StunMsgAttr
    {
        StunSourceAddressAttr() : StunMsgAttr(STUN_ATTR_SOURCE_ADDRESS)
        {
        }

        virtual ~StunSourceAddressAttr() = default;
        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };

};