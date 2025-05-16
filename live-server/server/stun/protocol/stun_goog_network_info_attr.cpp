#include "stun_goog_network_info_attr.h"
using namespace mms;

size_t StunGoogNetworkInfoAttr::size()
{
    return StunMsgAttr::size() + 4;
}

int32_t StunGoogNetworkInfoAttr::encode(uint8_t *data, size_t len)
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
    if (len < 4)
    {
        return -2;
    }

    *(uint16_t *)data = htons(network_id_);
    data += 2;
    *(uint16_t *)data = htons(network_cost_);
    data += 2;
    return data - data_start;
}

int32_t StunGoogNetworkInfoAttr::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;

    if (len <= 0)
    {
        return -2;
    }

    if (length != 4)
    {
        return -3;
    }

    network_id_ = ntohs(*(uint16_t *)data);
    data += 2;
    network_cost_ = ntohs(*(uint16_t *)data);
    data += 2;
    return data - data_start;
}