#pragma once

namespace mms
{
    //     0                   1                   2                   3
    //     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //    |x x x x x x x x|    Family     |           Port                |
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //    |                             Address                           |
    //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    struct StunMappedAddressAttr : public StunMsgAttr
    {
        StunMappedAddressAttr(uint32_t addr, uint16_t p) : StunMsgAttr(STUN_ATTR_MAPPED_ADDRESS), family(0x01)
        {
            address = addr;
            port = p;
        }

        StunMappedAddressAttr() : family(0x01)
        {
        }

        size_t size()
        {
            return StunMsgAttr::size() + 8;
        }

        int32_t encode(uint8_t *data, size_t len)
        {
            length = 8;
            uint8_t *data_start = data;
            int32_t consumed = StunMsgAttr::encode(data, len);
            if (consumed < 0)
            {
                return -1;
            }
            data += consumed;
            len -= consumed;
            if (len < 8)
            {
                return -2;
            }

            data[1] = family;
            data[0] = 0;
            data += 2;
            *(uint16_t *)data = htons(port);
            data += 2;
            *(uint32_t *)data = htonl(address);
            data += 4;
            return data - data_start;
        }

        int32_t decode(uint8_t *data, size_t len)
        {
            uint8_t *data_start = data;
            int32_t consumed = StunMsgAttr::decode(data, len);
            if (consumed < 0)
            {
                return -1;
            }
            data += consumed;
            len -= consumed;

            if (len < 8)
            {
                return -2;
            }
            family = data[1];
            data += 2;
            port = ntohs(*(uint16_t *)data);
            data += 2;
            address = ntohl(*(uint32_t *)data);
            data += 4;
            return data - data_start;
        }

        uint8_t family; // always 0x01
        uint16_t port;
        uint32_t address;
    };
};