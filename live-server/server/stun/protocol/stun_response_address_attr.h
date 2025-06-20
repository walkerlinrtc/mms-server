#pragma once
#include "stun_msg_attr.h"
#include "stun_define.hpp"
namespace mms
{
    struct StunResponseAddressAttr : public StunMsgAttr
    {
        StunResponseAddressAttr() : StunMsgAttr(STUN_ATTR_RESPONSE_ADDRESS)
        {
        }

        virtual ~StunResponseAddressAttr() = default;

        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };
};