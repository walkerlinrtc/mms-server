// RFC 5389                          STUN                      October 2008


// 15.5.  FINGERPRINT

//    The FINGERPRINT attribute MAY be present in all STUN messages.  The
//    value of the attribute is computed as the CRC-32 of the STUN message
//    up to (but excluding) the FINGERPRINT attribute itself, XOR'ed with
//    the 32-bit value 0x5354554e (the XOR helps in cases where an
//    application packet is also using CRC-32 in it).  The 32-bit CRC is
//    the one defined in ITU V.42 [ITU.V42.2002], which has a generator
//    polynomial of x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x+1.
//    When present, the FINGERPRINT attribute MUST be the last attribute in
//    the message, and thus will appear after MESSAGE-INTEGRITY.

//    The FINGERPRINT attribute can aid in distinguishing STUN packets from
//    packets of other protocols.  See Section 8.

//    As with MESSAGE-INTEGRITY, the CRC used in the FINGERPRINT attribute
//    covers the length field from the STUN message header.  Therefore,
//    this value must be correct and include the CRC attribute as part of
//    the message length, prior to computation of the CRC.  When using the
//    FINGERPRINT attribute in a message, the attribute is first placed
//    into the message with a dummy value, then the CRC is computed, and
//    then the value of the attribute is updated.  If the MESSAGE-INTEGRITY
//    attribute is also present, then it must be present with the correct
//    message-integrity value before the CRC is computed, since the CRC is
//    done over the value of the MESSAGE-INTEGRITY attribute as well.

#pragma once
#include <string>
#include "stun_define.hpp"
#include "base/utils/utils.h"
#include <iostream>
namespace mms {
    struct StunFingerPrintAttr : public StunMsgAttr
    {
        StunFingerPrintAttr(uint8_t *data, size_t len) : StunMsgAttr(STUN_ATTR_FINGERPRINT)
        {
            crc32 = Utils::get_crc32(data, len) ^ 0x5354554e;
        }

        StunFingerPrintAttr() = default;
    
        virtual ~StunFingerPrintAttr() = default;
        
        size_t size()
        {
            return StunMsgAttr::size() + sizeof(uint32_t);
        }

        int32_t encode(uint8_t *data, size_t len)
        {
            length = sizeof(uint32_t);
            uint8_t *data_start = data;
            int32_t consumed = StunMsgAttr::encode(data, len);
            if (consumed < 0)
            {
                return -1;
            }
            data += consumed;
            len -= consumed;
            if (len < sizeof(uint32_t))
            {
                return -2;
            }
            *(uint32_t *)data = htonl(crc32);
            data += sizeof(uint32_t);
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

            if (len < sizeof(uint32_t))
            {
                return -2;
            }

            if (length != sizeof(uint32_t))
            {
                return -3;
            }

            crc32 = ntohl(*(uint32_t *)data);
            data += 4;
            return data - data_start;
        }

        bool check(uint8_t *data, size_t len)
        {
            len -= 8;//去掉fingerprint本身长度
            uint32_t crc32_check = Utils::get_crc32(data, len) ^ 0x5354554e;
            return crc32 == crc32_check;
        }

        uint32_t crc32;
    };
};