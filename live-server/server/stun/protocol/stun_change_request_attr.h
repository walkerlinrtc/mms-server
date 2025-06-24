#pragma once
namespace mms {
    struct StunChangeRequestAttr : public StunMsgAttr
    {
        StunChangeRequestAttr() : StunMsgAttr(STUN_ATTR_CHANGE_REQUEST)
        {
        }

        virtual ~StunChangeRequestAttr() = default;
        
        bool change_ip = false;
        bool change_port = false;
    };
};