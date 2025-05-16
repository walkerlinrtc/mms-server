#include "stun_message_integrity_attr.h"
using namespace mms;
// 15.4.  MESSAGE-INTEGRITY

//    The MESSAGE-INTEGRITY attribute contains an HMAC-SHA1 [RFC2104] of
//    the STUN message.  The MESSAGE-INTEGRITY attribute can be present in
//    any STUN message type.  Since it uses the SHA1 hash, the HMAC will be
//    20 bytes.  The text used as input to HMAC is the STUN message,
//    including the header, up to and including the attribute preceding the
//    MESSAGE-INTEGRITY attribute.  With the exception of the FINGERPRINT
//    attribute, which appears after MESSAGE-INTEGRITY, agents MUST ignore
//    all other attributes that follow MESSAGE-INTEGRITY.




// Rosenberg, et al.           Standards Track                    [Page 34]

// RFC 5389                          STUN                      October 2008


//    The key for the HMAC depends on whether long-term or short-term
//    credentials are in use.  For long-term credentials, the key is 16
//    bytes:

//             key = MD5(username ":" realm ":" SASLprep(password))

//    That is, the 16-byte key is formed by taking the MD5 hash of the
//    result of concatenating the following five fields: (1) the username,
//    with any quotes and trailing nulls removed, as taken from the
//    USERNAME attribute (in which case SASLprep has already been applied);
//    (2) a single colon; (3) the realm, with any quotes and trailing nulls
//    removed; (4) a single colon; and (5) the password, with any trailing
//    nulls removed and after processing using SASLprep.  For example, if
//    the username was 'user', the realm was 'realm', and the password was
//    'pass', then the 16-byte HMAC key would be the result of performing
//    an MD5 hash on the string 'user:realm:pass', the resulting hash being
//    0x8493fbc53ba582fb4c044c456bdc40eb.

//    For short-term credentials:

//                           key = SASLprep(password)

//    where MD5 is defined in RFC 1321 [RFC1321] and SASLprep() is defined
//    in RFC 4013 [RFC4013].

//    The structure of the key when used with long-term credentials
//    facilitates deployment in systems that also utilize SIP.  Typically,
//    SIP systems utilizing SIP's digest authentication mechanism do not
//    actually store the password in the database.  Rather, they store a
//    value called H(A1), which is equal to the key defined above.

//    Based on the rules above, the hash used to construct MESSAGE-
//    INTEGRITY includes the length field from the STUN message header.
//    Prior to performing the hash, the MESSAGE-INTEGRITY attribute MUST be
//    inserted into the message (with dummy content).  The length MUST then
//    be set to point to the length of the message up to, and including,
//    the MESSAGE-INTEGRITY attribute itself, but excluding any attributes
//    after it.  Once the computation is performed, the value of the
//    MESSAGE-INTEGRITY attribute can be filled in, and the value of the
//    length in the STUN header can be set to its correct value -- the
//    length of the entire message.  Similarly, when validating the
//    MESSAGE-INTEGRITY, the length field should be adjusted to point to
//    the end of the MESSAGE-INTEGRITY attribute prior to calculating the
//    HMAC.  Such adjustment is necessary when attributes, such as
//    FINGERPRINT, appear after MESSAGE-INTEGRITY.

#include "stun_msg.h"
StunMessageIntegrityAttr::StunMessageIntegrityAttr(uint8_t *data, size_t len, bool has_finger_print, const std::string & pwd) : StunMsgAttr(STUN_ATTR_MESSAGE_INTEGRITY)
{
    unsigned int digest_len;
    hmac_sha1.resize(20);
    uint16_t len_backup = *(uint16_t *)(data+2);
    size_t content_len_with_message_integrity = len - 20;// 20 byte header（减掉20字节的头部）
    if (has_finger_print) {
        content_len_with_message_integrity -= 8;//4byte attr header + 4 byte finger print data
    }
    *(uint16_t *)(data+2) = htons(content_len_with_message_integrity);
    HMAC_CTX *ctx = HMAC_CTX_new();
    HMAC_Init_ex(ctx, pwd.c_str(), pwd.size(), EVP_sha1(), NULL);
    HMAC_Update(ctx, (const unsigned char *)data, content_len_with_message_integrity + 20 - 24);
    HMAC_Final(ctx, (unsigned char *)hmac_sha1.data(), &digest_len);
    HMAC_CTX_free(ctx);
    *(uint16_t *)(data+2) = len_backup;
}

size_t StunMessageIntegrityAttr::size()
{
    return StunMsgAttr::size() + 20;
}

int32_t StunMessageIntegrityAttr::encode(uint8_t *data, size_t len)
{
    length = 20;
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::encode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;
    if (len < 20)
    {
        return -2;
    }
    memcpy(data, hmac_sha1.data(), 20);
    data += 20;
    return data - data_start;
}

int32_t StunMessageIntegrityAttr::decode(uint8_t *data, size_t len)
{
    uint8_t *data_start = data;
    int32_t consumed = StunMsgAttr::decode(data, len);
    if (consumed < 0)
    {
        return -1;
    }
    data += consumed;
    len -= consumed;

    if (length != 20)
    {
        return -2;
    }

    if (len < 20) {
        return -3;
    }

    hmac_sha1.assign((char*)data, length);
    data += length;
    // len -= consumed;
    return data - data_start;
}

bool StunMessageIntegrityAttr::check(StunMsg & stun_msg, uint8_t *data, size_t len, const std::string & pwd)
{
    unsigned int digest_len;
    std::string hmac_sha1_check;
    hmac_sha1_check.resize(20);
    uint16_t len_backup = *(uint16_t *)(data+2);//先把原来的长度备份下
    size_t content_len_with_message_integrity = len - 20;// 20 byte header（减掉20字节的头部）
    if (stun_msg.fingerprint_attr) {
        content_len_with_message_integrity -= 8;//4byte attr header + 4 byte finger print data
    }
    *(uint16_t *)(data+2) = htons(content_len_with_message_integrity);
    HMAC_CTX *ctx = HMAC_CTX_new();
    HMAC_Init_ex(ctx, pwd.c_str(), pwd.size(), EVP_sha1(), NULL);
    HMAC_Update(ctx, (const unsigned char *)data, content_len_with_message_integrity + 20 - 24);//把头部长度加上，再减掉message integrity attr自己的长度
    HMAC_Final(ctx, (unsigned char *)hmac_sha1_check.data(), &digest_len);
    HMAC_CTX_free(ctx);
    *(uint16_t *)(data+2) = len_backup;//还原原来的长度
    if (hmac_sha1_check == hmac_sha1) {
        return true;
    }
    return false;
}