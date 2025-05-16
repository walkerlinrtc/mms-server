#pragma once
namespace mms {
struct StunErrorCodeAttr : public StunMsgAttr
    {
        StunErrorCodeAttr(int32_t code, const std::string &reason) : StunMsgAttr(STUN_ATTR_ERROR_CODE)
        {
            (void)reason;
            _class = (uint8_t)(code / 100);
            number = (uint8_t)(code % 100);
        }

        size_t size()
        {
            return StunMsgAttr::size() + 4 + reason.size();
        }

        int32_t encode(uint8_t *data, size_t len)
        {
            length = 4 + reason.size();
            uint8_t *data_start = data;
            int32_t consumed = StunMsgAttr::encode(data, len);
            if (consumed < 0)
            {
                return -1;
            }
            data += consumed;
            len -= consumed;
            if (len < 4 + reason.size())
            {
                return -2;
            }

            data[3] = number;
            data[2] = 0x07 & _class;
            data[1] = 0;
            data[0] = 0;
            data += 4;
            if (reason.size() > 0)
            {
                memcpy(data, reason.data(), reason.size());
                data += reason.size();
            }

            return data - data_start;
        }

        uint8_t _class;
        uint8_t number;
        std::string reason;
    };

};