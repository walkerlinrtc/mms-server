// The ICE-CONTROLLED attribute is present in a Binding request and
//    indicates that the client believes it is currently in the controlled
//    role.  The content of the attribute is a 64-bit unsigned integer in
//    network byte order, which contains a random number used for tie-
//    breaking of role conflicts.
#pragma once
#include <stdint.h>
#include "stun_define.hpp"

namespace mms {
struct StunIceControllingAttr : public StunMsgAttr
{
    StunIceControllingAttr(uint32_t rand) : StunMsgAttr(STUN_ICE_ATTR_ICE_CONTROLLING), tie_breaker_(rand)
    {
        
    }

    StunIceControllingAttr() = default;
    virtual ~StunIceControllingAttr() = default;

    size_t size();

    int32_t encode(uint8_t *data, size_t len);

    int32_t decode(uint8_t *data, size_t len);

    uint64_t get_tie_breaker() const {
        return tie_breaker_;
    }

    void set_tie_breaker(uint64_t val) {
        tie_breaker_ = val;
    }
private:
    uint64_t tie_breaker_ = 0;
};
};