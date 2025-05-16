#include <iostream>
#include "stun_ice_controlled_attr.h"
using namespace mms;

size_t StunIceControlledAttr::size()
{
    return StunMsgAttr::size() + 4;
}

int32_t StunIceControlledAttr::encode(uint8_t *data, size_t len)
{
    length = sizeof(uint64_t);
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::encode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;
    if (len < length)
    {
        return -2;
    }
    
    *(uint64_t *)data = htobe64(tie_breaker_);
    data += sizeof(uint64_t);
    return data - data_start;
}

int32_t StunIceControlledAttr::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;

    if (len < 4)
    {
        return -2;
    }

    if (length != sizeof(uint64_t)) 
    {
        return -3;
    }

    tie_breaker_ = be64toh(*(uint64_t *)data);
    data += length;
    return data - data_start;
}