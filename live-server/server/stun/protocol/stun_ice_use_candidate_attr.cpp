#include <iostream>
#include "stun_ice_use_candidate_attr.h"
using namespace mms;

size_t StunIceUseCandidateAttr::size()
{
    return StunMsgAttr::size();
}

int32_t StunIceUseCandidateAttr::encode(uint8_t *data, size_t len)
{
    length = 0;
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::encode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;
    return data - data_start;
}

int32_t StunIceUseCandidateAttr::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;
    return data - data_start;
}