#pragma once
#include <string>
namespace mms {
    struct StunPasswordAttr : public StunMsgAttr
    {
        StunPasswordAttr() : StunMsgAttr(STUN_ATTR_PASSWORD)
        {
        }

        virtual ~StunPasswordAttr() = default;

        std::string password;
    };
};