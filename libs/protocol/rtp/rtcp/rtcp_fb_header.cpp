#include <iostream>
#include <arpa/inet.h>
#include "rtcp_fb_header.h"
using namespace mms;
int32_t RtcpFbHeader::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    if (len < 1)
    {
        return -1;
    }

    uint8_t tmpv1 = *data;
    fmt = (tmpv1 & 0x1F);
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
    // media source ssrc
    if (len < 4) {
        return -5;
    }
    media_source_ssrc = ntohl(*(uint32_t*)data);
    data += 4;

    return data - data_start;
}

int32_t RtcpFbHeader::encode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    if (len < 1)
    {
        return -1;
    }

    uint8_t tmpv1 = (version << 6) | (padding << 5) | fmt;
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

    if (len < 4) {
        return -5;
    }
     *(uint32_t*)data = htonl(media_source_ssrc);
    data += 4;
    len -= 4;

    return data - data_start;
}