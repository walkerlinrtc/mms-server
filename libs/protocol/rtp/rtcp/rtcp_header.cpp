#include <iostream>
#include <arpa/inet.h>
#include "rtcp_header.h"
using namespace mms;
RtcpHeader::RtcpHeader() {
    version = 2;
    padding = 0;
}

bool RtcpHeader::is_rtcp_packet(uint8_t *data, size_t len)
{
    if (len < 2)
    {
        return false;
    }
    bool is_rtp_or_rtcp = (len >= 12 && ((data[0] & 0xC0) == 0x80));
    bool is_rtcp_pt     = (data[1] >= 192 && data[1] <= 223);
    
    return is_rtp_or_rtcp && is_rtcp_pt;
}

int32_t RtcpHeader::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    if (len < 1)
    {
        return -1;
    }

    uint8_t tmpv1 = *data;
    rc = (tmpv1 & 0x1F);
    padding = (tmpv1 >> 5) & 0x01;
    version = (tmpv1 >> 6) & 0x03;

    data++;
    len--;
    if (len < 1)
    {
        return -2;
    }

    pt = *data;
    data++;
    len--;
    // length
    if (len < 2)
    {
        return -3;
    }

    length = ntohs(*(uint16_t*)data);
    data += 2;
    len -= 2;
    // ssrc
    if (len < 4)
    {
        return -4;
    }

    sender_ssrc = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    return data - data_start;
}

int32_t RtcpHeader::encode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    if (len < 1)
    {
        return -1;
    }

    uint8_t tmpv1 = (version << 6) | (padding << 5) | rc;
    *data = tmpv1;
    data++;
    len--;
    if (len < 1)
    {
        return -2;
    }

    *data = pt;
    data++;
    len--;
    // length
    if (len < 2)
    {
        return -3;
    }

    *(uint16_t*)data = htons(length);
    data += 2;
    len -= 2;
    // ssrc
    if (len < 4)
    {
        return -4;
    }

    *(uint32_t*)data = htonl(sender_ssrc);
    data += 4;
    len -= 4;

    return data - data_start;
}

uint8_t RtcpHeader::parse_pt(uint8_t *data, size_t len)
{
    ((void)len);
    return data[1];
}