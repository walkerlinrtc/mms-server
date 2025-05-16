/*
@https://blog.csdn.net/u012538729/article/details/115694308
USERNAME：用户名，用于消息完整性，在webrtc中的规则为 “对端的ice-ufrag：自己的ice-ufrag”，其中ice-ufrag已通过提议/应答的SDP信息进行交互。


11.2.6 USERNAME

   The USERNAME attribute is used for message integrity.  It serves as a
   means to identify the shared secret used in the message integrity
   check.  The USERNAME is always present in a Shared Secret Response,
   along with the PASSWORD.  It is optionally present in a Binding
   Request when message integrity is used.





Rosenberg, et al.           Standards Track                    [Page 28]

RFC 3489                          STUN                        March 2003


   The value of USERNAME is a variable length opaque value.  Its length
   MUST be a multiple of 4 (measured in bytes) in order to guarantee
   alignment of attributes on word boundaries.







5.4.  "ice-ufrag" and "ice-pwd" Attributes

   The "ice-ufrag" and "ice-pwd" attributes convey the username fragment
   and password used by ICE for message integrity.  Their syntax is:

   ice-pwd-att           = "ice-pwd:" password
   ice-ufrag-att         = "ice-ufrag:" ufrag
   password              = 22*256ice-char
   ufrag                 = 4*256ice-char

   The "ice-pwd" and "ice-ufrag" attributes can appear at either the
   session-level or media-level.  When present in both, the value in the
   media-level takes precedence.  Thus, the value at the session-level
   is effectively a default that applies to all data streams, unless
   overridden by a media-level value.  Whether present at the session or
   media-level, there MUST be an ice-pwd and ice-ufrag attribute for
   each data stream.  If two data streams have identical ice-ufrag's,
   they MUST have identical ice-pwd's.

   The ice-ufrag and ice-pwd attributes MUST be chosen randomly at the
   beginning of a session (the same applies when ICE is restarting for
   an agent).

   [RFC8445] requires the ice-ufrag attribute to contain at least 24
   bits of randomness, and the ice-pwd attribute to contain at least 128
   bits of randomness.  This means that the ice-ufrag attribute will be
   at least 4 characters long, and the ice-pwd at least 22 characters
   long, since the grammar for these attributes allows for 6 bits of
   information per character.  The attributes MAY be longer than 4 and
   22 characters, respectively, of course, up to 256 characters.  The
   upper limit allows for buffer sizing in implementations.  Its large



Petit-Huguenin, et al.  Expires February 14, 2020              [Page 21]

Internet-Draft                ICE SDP Usage                  August 2019


   upper limit allows for increased amounts of randomness to be added
   over time.  For compatibility with the 512 character limitation for
   the STUN username attribute value and for bandwidth conservation
   considerations, the ice-ufrag attribute MUST NOT be longer than 32
   characters when sending, but an implementation MUST accept up to 256
   characters when receiving.

   Example shows sample ice-ufrag and ice-pwd SDP lines:

   a=ice-pwd:asd88fgpdd777uzjYhagZg
   a=ice-ufrag:8hhY
*/

#pragma once
#include <string>
#include "stun_define.hpp"
namespace mms
{
    struct StunUsernameAttr : public StunMsgAttr
    {
        StunUsernameAttr(const std::string &local_user_name, const std::string &remote_user_name = "") : StunMsgAttr(STUN_ATTR_USERNAME), local_user_name_(local_user_name), remote_user_name_(remote_user_name)
        {
        }

        StunUsernameAttr() = default;

        size_t size();

        int32_t encode(uint8_t *data, size_t len);

        int32_t decode(uint8_t *data, size_t len);

        const std::string &get_local_user_name() const
        {
            return local_user_name_;
        }

        void set_local_user_name(const std::string &username)
        {
            local_user_name_ = username;
        }

        const std::string &get_remote_user_name() const
        {
            return remote_user_name_;
        }

        void set_remote_user_name(const std::string &username)
        {
            remote_user_name_ = username;
        }

    private:
        std::string local_user_name_;
        std::string remote_user_name_;
    };
};