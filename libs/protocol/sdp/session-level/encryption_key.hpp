#pragma once
#include <string>
// 5.12.  Encryption Keys ("k=")

//       k=<method>
//       k=<method>:<encryption key>

//    If transported over a secure and trusted channel, the Session
//    Description Protocol MAY be used to convey encryption keys.  A simple
//    mechanism for key exchange is provided by the key field ("k="),
//    although this is primarily supported for compatibility with older
//    implementations and its use is NOT RECOMMENDED.  Work is in progress
//    to define new key exchange mechanisms for use with SDP [27] [28], and
//    it is expected that new applications will use those mechanisms.
//    A key field is permitted before the first media entry (in which case
//    it applies to all media in the session), or for each media entry as
//    required.  The format of keys and their usage are outside the scope
//    of this document, and the key field provides no way to indicate the
//    encryption algorithm to be used, key type, or other information about
//    the key: this is assumed to be provided by the higher-level protocol
//    using SDP.  If there is a need to convey this information within SDP,
//    the extensions mentioned previously SHOULD be used.  Many security
//    protocols require two keys: one for confidentiality, another for
//    integrity.  This specification does not support transfer of two keys.

//    The method indicates the mechanism to be used to obtain a usable key
//    by external means, or from the encoded encryption key given.  The
//    following methods are defined:

//       k=clear:<encryption key>

//          The encryption key is included untransformed in this key field.
//          This method MUST NOT be used unless it can be guaranteed that
//          the SDP is conveyed over a secure channel.  The encryption key
//          is interpreted as text according to the charset attribute; use
//          the "k=base64:" method to convey characters that are otherwise
//          prohibited in SDP.

//       k=base64:<encoded encryption key>

//          The encryption key is included in this key field but has been
//          base64 encoded [12] because it includes characters that are
//          prohibited in SDP.  This method MUST NOT be used unless it can
//          be guaranteed that the SDP is conveyed over a secure channel.

//       k=uri:<URI to obtain key>

//          A Uniform Resource Identifier is included in the key field.
//          The URI refers to the data containing the key, and may require
//          additional authentication before the key can be returned.  When
//          a request is made to the given URI, the reply should specify
//          the encoding for the key.  The URI is often an Secure Socket
//          Layer/Transport Layer Security (SSL/TLS)-protected HTTP URI
//          ("https:"), although this is not required.

//       k=prompt

//          No key is included in this SDP description, but the session or
//          media stream referred to by this key field is encrypted.  The
//          user should be prompted for the key when attempting to join the
//          session, and this user-supplied key should then be used to
//          decrypt the media streams.  The use of user-specified keys is
//          NOT RECOMMENDED, since such keys tend to have weak security
//          properties.

//    The key field MUST NOT be used unless it can be guaranteed that the
//    SDP is conveyed over a secure and trusted channel.  An example of
//    such a channel might be SDP embedded inside an S/MIME message or a
//    TLS-protected HTTP session.  It is important to ensure that the
//    secure channel is with the party that is authorised to join the
//    session, not an intermediary: if a caching proxy server is used, it
//    is important to ensure that the proxy is either trusted or unable to
//    access the SDP.

namespace mms {
struct EncryptionKeys {
public:
    std::string raw_string;
};
};