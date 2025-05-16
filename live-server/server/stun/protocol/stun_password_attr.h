#pragma once
namespace mms {
    struct StunPasswordAttr : public StunMsgAttr
    {
        StunPasswordAttr() : StunMsgAttr(STUN_ATTR_PASSWORD)
        {
        }

        std::string password;
    };
};