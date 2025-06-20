#include <iostream>
#include "stun_software_attr.h"
using namespace mms;

size_t StunSoftwareAttr::size()
{
    return StunMsgAttr::size() + 4;
}

int32_t StunSoftwareAttr::encode(uint8_t *data, size_t len)
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
    if (len < val_.size())
    {
        return -2;
    }
    
    memcpy(data, val_.data(), val_.size());
    data += val_.size();
    return data - data_start;
}

int32_t StunSoftwareAttr::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;

    if (len < length) 
    {
        return -3;
    }

    val_ = std::string((char*)data, length);
    data += length;
    return data - data_start;
}