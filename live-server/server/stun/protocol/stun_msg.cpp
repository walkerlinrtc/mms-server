#include <iostream>
#include "spdlog/spdlog.h"

#include "stun_define.hpp"
#include "stun_msg.h"

#include "stun_message_integrity_attr.h"
#include "stun_goog_network_info_attr.h"
#include "stun_ice_priority_attr.h"
#include "stun_ice_use_candidate_attr.h"
#include "stun_ice_controlled_attr.h"
#include "stun_ice_controlling_attr.h"
#include "stun_message_integrity_attr.h"
#include "stun_fingerprint_attr.h"
#include "stun_mapped_address_attr.h"
#include "stun_password_attr.h"
#include "stun_reflect_from_attr.h"
#include "stun_response_address_attr.h"
#include "stun_source_address_attr.h"
#include "stun_unknown_attributes_attr.h"
#include "stun_change_address_attr.h"
#include "stun_error_code_attr.h"

// 7.3.  Receiving a STUN Message

//    This section specifies the processing of a STUN message.  The
//    processing specified here is for STUN messages as defined in this
//    specification; additional rules for backwards compatibility are
//    defined in Section 12.  Those additional procedures are optional, and
//    usages can elect to utilize them.  First, a set of processing
//    operations is applied that is independent of the class.  This is
//    followed by class-specific processing, described in the subsections
//    that follow.

// Rosenberg, et al.           Standards Track                    [Page 16]

// RFC 5389                          STUN                      October 2008

//    When a STUN agent receives a STUN message, it first checks that the
//    message obeys the rules of Section 6.  It checks that the first two
//    bits are 0, that the magic cookie field has the correct value, that
//    the message length is sensible, and that the method value is a
//    supported method.  It checks that the message class is allowed for
//    the particular method.  If the message class is "Success Response" or
//    "Error Response", the agent checks that the transaction ID matches a
//    transaction that is still in progress.  If the FINGERPRINT extension
//    is being used, the agent checks that the FINGERPRINT attribute is
//    present and contains the correct value.  If any errors are detected,
//    the message is silently discarded.  In the case when STUN is being
//    multiplexed with another protocol, an error may indicate that this is
//    not really a STUN message; in this case, the agent should try to
//    parse the message as a different protocol.

using namespace mms;
int32_t StunMsg::decode(uint8_t *data, size_t len)
{
    int32_t consumed = header.decode(data, len);
    if (consumed < 0)
    {
        spdlog::error("decode stun msg header failed.");
        return -1;
    }

    data += consumed;
    len -= consumed;
    while (len > 0)
    {
        uint16_t t = ntohs(*(uint16_t *)data);
        switch (t)
        {
        case STUN_ATTR_MAPPED_ADDRESS:
        {
            auto mapped_addr_attr = std::unique_ptr<StunMappedAddressAttr>(new StunMappedAddressAttr);
            int32_t c = mapped_addr_attr->decode(data, len);
            if (c < 0)
            {
                return -2;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(mapped_addr_attr));
            break;
        }
        case STUN_ATTR_RESPONSE_ADDRESS:
        {
            break;
        }
        case STUN_ATTR_CHANGE_REQUEST:
        {
            break;
        }
        case STUN_ATTR_SOURCE_ADDRESS:
        {
            break;
        }
        case STUN_ATTR_CHANGED_ADDRESS:
        {
            break;
        }
        case STUN_ATTR_USERNAME:
        {
            StunUsernameAttr username;
            int32_t c = username.decode(data, len);
            if (c < 0)
            {
                return -3;
            }
            username_attr = username;
            data += c;
            len -= c;
            break;
        }
        case STUN_ATTR_PASSWORD:
        {
            break;
        }
        case STUN_ATTR_MESSAGE_INTEGRITY:
        {
            /*
            With the exception of the FINGERPRINT
            attribute, which appears after MESSAGE-INTEGRITY, agents MUST ignore
            all other attributes that follow MESSAGE-INTEGRITY.
            */
            msg_integrity_attr = std::unique_ptr<StunMessageIntegrityAttr>(new StunMessageIntegrityAttr);
            int32_t c = msg_integrity_attr->decode(data, len);
            if (c < 0)
            {
                return -4;
            }
            data += c;
            len -= c;
            break;
        }
        case STUN_ATTR_ERROR_CODE:
        {
            break;
        }
        case STUN_ATTR_UNKNOWN_ATTRIBUTES:
        {
            break;
        }
        case STUN_ATTR_REFLECTED_FROM:
        {
            break;
        }
        case STUN_ATTR_GOOG_NETWORK_INFO:
        {
            auto goog_network_info_attr = std::unique_ptr<StunGoogNetworkInfoAttr>(new StunGoogNetworkInfoAttr);
            int32_t c = goog_network_info_attr->decode(data, len);
            if (c < 0)
            {
                return -5;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(goog_network_info_attr));
            break;
        }
        case STUN_ICE_ATTR_PRIORITY:
        {
            auto ice_priority_attr = std::unique_ptr<StunIcePriorityAttr>(new StunIcePriorityAttr);
            int32_t c = ice_priority_attr->decode(data, len);
            if (c < 0)
            {
                return -6;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(ice_priority_attr));
            break;
        }
        case STUN_ICE_ATTR_USE_CANDIDATE:
        {
            auto ice_use_candidate_attr = std::unique_ptr<StunIceUseCandidateAttr>(new StunIceUseCandidateAttr);
            int32_t c = ice_use_candidate_attr->decode(data, len);
            if (c < 0)
            {
                return -7;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(ice_use_candidate_attr));
            break;
        }
        case STUN_ICE_ATTR_ICE_CONTROLLED:
        {
            auto ice_controlled_attr = std::unique_ptr<StunIceControlledAttr>(new StunIceControlledAttr);
            int32_t c = ice_controlled_attr->decode(data, len);
            if (c < 0)
            {
                return -8;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(ice_controlled_attr));
            break;
        }
        case STUN_ICE_ATTR_ICE_CONTROLLING:
        {
            auto ice_controlling_attr = std::unique_ptr<StunIceControllingAttr>(new StunIceControllingAttr);
            int32_t c = ice_controlling_attr->decode(data, len);
            if (c < 0)
            {
                return -9;
            }
            data += c;
            len -= c;
            attrs.emplace_back(std::move(ice_controlling_attr));
            break;
        }
        default:
        {
            spdlog::error("decode attr failed, type:{0:x}", t);
            return -10;
        }
        }

        /*
            With the exception of the FINGERPRINT
            attribute, which appears after MESSAGE-INTEGRITY, agents MUST ignore
            all other attributes that follow MESSAGE-INTEGRITY.
        */
        if (msg_integrity_attr)
        { //如果解析到message integrity了，则最后只解析fingerprint了
            break;
        }
    }

    if (len > 0)
    {
        uint16_t t = ntohs(*(uint16_t *)data);
        if (STUN_ATTR_FINGERPRINT == t)
        {
            fingerprint_attr = std::unique_ptr<StunFingerPrintAttr>(new StunFingerPrintAttr);
            int32_t c = fingerprint_attr->decode(data, len);
            if (c < 0)
            {
                return -11;
            }
            data += c;
            len -= c;
        }
    }

    return 0;
}

size_t StunMsg::size(bool add_message_integrity, bool add_finger_print)
{
    int32_t s = 0;
    s += header.size();
    for (auto &attr : attrs)
    {
        s += attr->size();
    }

    if (username_attr)
    {
        s += username_attr.value().size();
    }

    if (add_message_integrity) 
    {
        s += 24;/*StunMsgAttr::size() + 20bytes*/
    }

    if (add_finger_print)
    {
        s += 4 + 4; /*StunMsgAttr::size() + crc32*/
    }
    return s;
}

int32_t StunMsg::encode(uint8_t *data, size_t len, bool add_message_integrity, const std::string &pwd, bool add_finger_print)
{
    int32_t content_len = 0;
    if (username_attr)
    {
        content_len += username_attr.value().size();
    }

    for (auto &attr : attrs)
    {
        content_len += attr->size();
    }
    header.length = content_len;

    if (add_message_integrity)
    {
        header.length += 4 + 20;
    }

    if (add_finger_print)
    {
        header.length += 8;
    }

    uint8_t *data_start = data;
    int32_t consumed = header.encode(data, len);
    if (consumed < 0)
    {
        return -1;
    }

    data += consumed;
    len -= consumed;

    if (username_attr)
    {
        consumed = username_attr.value().encode(data, len);
        if (consumed < 0)
        {
            return -1;
        }
        data += consumed;
        len -= consumed;
    }

    for (auto &attr : attrs)
    {
        consumed = attr->encode(data, len);
        if (consumed < 0)
        {
            return -2;
        }
        data += consumed;
        len -= consumed;
    }

    if (add_message_integrity)
    {
        msg_integrity_attr = std::unique_ptr<StunMessageIntegrityAttr>(new StunMessageIntegrityAttr(data_start, header.length + 20, add_finger_print, pwd));
        consumed = msg_integrity_attr->encode(data, len);
        if (consumed < 0)
        {
            return -3;
        }
        data += consumed;
        len -= consumed;
    }

    if (add_finger_print)
    {
        fingerprint_attr = std::unique_ptr<StunFingerPrintAttr>(new StunFingerPrintAttr(data_start, data - data_start));
        consumed = fingerprint_attr->encode(data, len);
        if (consumed < 0)
        {
            return -4;
        }
        data += consumed;
        len -= consumed;
    }

    return data - data_start;
}

bool StunMsg::check_msg_integrity(uint8_t *data, size_t len, const std::string &pwd)
{
    if (!msg_integrity_attr->check(*this, data, len, pwd))
    {
        return false;
    }
    return true;
}

bool StunMsg::check_finger_print(uint8_t *data, size_t len) 
{
    if (!fingerprint_attr) 
    {
        return true;
    }
    return fingerprint_attr->check(data, len);
}
