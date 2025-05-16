#pragma once
namespace mms {
    struct StunChangeRequestAttr : public StunMsgAttr
    {
        StunChangeRequestAttr() : StunMsgAttr(STUN_ATTR_CHANGE_REQUEST)
        {
        }

        bool change_ip = false;
        bool change_port = false;
    };
};