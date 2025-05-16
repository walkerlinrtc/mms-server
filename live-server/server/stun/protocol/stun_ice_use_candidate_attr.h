/*
The USE-CANDIDATE attribute indicates that the candidate pair
   resulting from this check should be used for transmission of media.
   The attribute has no content (the Length field of the attribute is
   zero); it serves as a flag.  It has an attribute value of 0x0025.

   The controlling agent MAY include the USE-CANDIDATE attribute in the
   Binding request.  The controlled agent MUST NOT include it in its
   Binding request.  This attribute signals that the controlling agent
   wishes to cease checks for this component, and use the candidate pair
   resulting from the check for this component.  Section 8.1.1 provides
   guidance on determining when to include it.
*/
#pragma once
#include "stun_define.hpp"
namespace mms {
struct StunIceUseCandidateAttr : public StunMsgAttr
{
    StunIceUseCandidateAttr() : StunMsgAttr(STUN_ICE_ATTR_USE_CANDIDATE)
    {
        
    }

    size_t size();

    int32_t encode(uint8_t *data, size_t len);

    int32_t decode(uint8_t *data, size_t len);
};
};