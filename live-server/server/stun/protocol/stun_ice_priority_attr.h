/*
The PRIORITY attribute indicates the priority that is to be
   associated with a peer reflexive candidate, should one be discovered
   by this check.  It is a 32-bit unsigned integer, and has an attribute
   value of 0x0024.

7.1.2.1.  PRIORITY and USE-CANDIDATE

   An agent MUST include the PRIORITY attribute in its Binding request.
   The attribute MUST be set equal to the priority that would be
   assigned, based on the algorithm in Section 4.1.2, to a peer
   reflexive candidate, should one be learned as a consequence of this
   check (see Section 7.1.3.2.1 for how peer reflexive candidates are
   learned).  This priority value will be computed identically to how
   the priority for the local candidate of the pair was computed, except
   that the type preference is set to the value for peer reflexive
   candidate types.
*/
#pragma once
#include "stun_define.hpp"
namespace mms {
struct StunIcePriorityAttr : public StunMsgAttr
{
    StunIcePriorityAttr(uint32_t priority) : StunMsgAttr(STUN_ICE_ATTR_PRIORITY), priority_(priority)
    {
        
    }

    StunIcePriorityAttr() = default;
    virtual ~StunIcePriorityAttr() = default;

    size_t size();

    int32_t encode(uint8_t *data, size_t len);

    int32_t decode(uint8_t *data, size_t len);

    uint32_t get_priority() const {
        return priority_;
    }

    void set_priority(uint32_t val) {
        priority_ = val;
    }
private:
    uint32_t priority_ = 0;
};
};