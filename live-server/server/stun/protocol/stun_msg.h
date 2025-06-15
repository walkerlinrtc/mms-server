#pragma once
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <netinet/in.h>
#include <string.h>

#include "stun_message_integrity_attr.h"
#include "stun_fingerprint_attr.h"
#include "stun_username_attr.h"

/*
RFC3489中定义：
11.1  Message Header

   All STUN messages consist of a 20 byte header:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      STUN Message Type        |         Message Length        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                            Transaction ID
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

/*
RFC5389中定义：
   STUN messages MUST start with a 20-byte header followed by zero
   or more Attributes.  The STUN header contains a STUN message type,
   magic cookie, transaction ID, and message length.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |0 0|     STUN Message Type     |         Message Length        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                         Magic Cookie                          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                     Transaction ID (96 bits)                  |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
// 现在应该都用RFC5389的格式了

#define STUN_MAGIC_COOKIE 0x2112A442
namespace mms
{
    struct StunMsgHeader
    {
        uint16_t type;
        uint16_t length;
        uint32_t magic_cookie = STUN_MAGIC_COOKIE;
        uint8_t transaction_id[12];
        int32_t decode(uint8_t *data, size_t len)
        {
            if (len < 20)
            {
                return -1;
            }

            type = ntohs(*(uint16_t *)data);
            if ((type & 0xC0) != 0)
            {
                return -2;
            }

            data += 2;
            len = ntohs(*(uint16_t *)data);
            data += 2;
            magic_cookie = ntohl(*(uint32_t *)data);
            if (magic_cookie != STUN_MAGIC_COOKIE)
            {
                return -2;
            }
            data += 4;
            memcpy(transaction_id, data, 12);
            return 20;
        }

        int32_t encode(uint8_t *data, size_t len)
        {
            if (len < 20)
            {
                return -1;
            }
            *(uint16_t *)data = htons(type);
            data += 2;
            *(uint16_t *)data = htons(length);
            data += 2;
            *(uint32_t *)data = htonl(magic_cookie);
            data += 4;
            memcpy(data, transaction_id, 12);
            return 20;
        }

        size_t size()
        {
            return 20;
        }
    };

    struct StunMsg
    {
        StunMsg() = default;
        virtual ~StunMsg() = default;
        static bool isStunMsg(const uint8_t *data, size_t len);
        StunMsgHeader header;
        std::vector<std::unique_ptr<StunMsgAttr>> attrs;
        std::optional<StunUsernameAttr> username_attr;
        std::unique_ptr<StunMessageIntegrityAttr> msg_integrity_attr;
        std::unique_ptr<StunFingerPrintAttr> fingerprint_attr;

        void add_attr(std::unique_ptr<StunMsgAttr> attr)
        {
            attrs.emplace_back(std::move(attr));
        }

        uint16_t type()
        {
            return header.type;
        }

        bool has_msg_integrity_attr() const
        {
            return msg_integrity_attr != nullptr;
        }

        bool has_finger_print() const
        {
            return fingerprint_attr != nullptr;
        }

        int32_t decode(uint8_t *data, size_t len);

        size_t size(bool add_message_integrity = false, bool add_finger_print = false);

        virtual int32_t encode(uint8_t *data, size_t len, bool add_message_integrity = false, const std::string &pwd = "", bool add_finger_print = false);

        bool check_msg_integrity(uint8_t *data, size_t len, const std::string &pwd);
        bool check_finger_print(uint8_t *data, size_t len);

        const std::optional<StunUsernameAttr> get_username_attr() const
        {
            return username_attr;
        }

        void set_username_attr(const StunUsernameAttr &username)
        {
            username_attr = username;
        }
    };
};