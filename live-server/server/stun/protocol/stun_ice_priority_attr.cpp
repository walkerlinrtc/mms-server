#include <iostream>
#include "stun_ice_priority_attr.h"
using namespace mms;

size_t StunIcePriorityAttr::size()
{
    return StunMsgAttr::size() + 4;
}

int32_t StunIcePriorityAttr::encode(uint8_t *data, size_t len)
{
    length = 4;
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
    
    *(uint32_t *)data = htonl(priority_);
    data += 4;
    return data - data_start;
}

int32_t StunIcePriorityAttr::decode(uint8_t *data, size_t len)
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

    if (length != 4) 
    {
        return -3;
    }

    priority_ = ntohl(*(uint32_t *)data);
    data += length;
    return data - data_start;
}