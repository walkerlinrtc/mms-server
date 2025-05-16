#pragma once

namespace mms
{
    struct StunMsgAttr
    {
        StunMsgAttr()
        {
        }

        StunMsgAttr(uint16_t t) : type(t)
        {
        }

        virtual size_t size()
        {
            return 4;
        }

        virtual int32_t encode(uint8_t *data, size_t len)
        {
            if (len < 4)
            {
                return -1;
            }
            *(uint16_t *)data = htons(type);
            data += 2;
            *(uint16_t *)data = htons(length);
            return 4;
        }

        virtual int32_t decode(uint8_t *data, size_t len)
        {
            if (len < 4)
            {
                return -1;
            }
            type = ntohs(*(uint16_t *)data);
            data += 2;
            length = ntohs(*(uint16_t *)data);
            data += 2;
            return 4;
        }

        uint16_t get_type()
        {
            return type;
        }

        uint16_t type;
        uint16_t length;
    };

};