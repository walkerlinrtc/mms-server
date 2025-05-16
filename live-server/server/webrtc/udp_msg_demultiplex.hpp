#pragma once
#include <stdint.h>
#include <stdlib.h>
// // @https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
// // NEW TEXT

// //    The process for demultiplexing a packet is as follows.  The receiver
// //    looks at the first byte of the packet.  If the value of this byte is
// //    in between 0 and 3 (inclusive), then the packet is STUN.  If the
// //    value is between 16 and 19 (inclusive), then the packet is ZRTP.  If
// //    the value is between 20 and 63 (inclusive), then the packet is DTLS.
// //    If the value is between 64 and 79 (inclusive), then the packet is
// //    TURN Channel.  If the value is in between 128 and 191 (inclusive),
// //    then the packet is RTP (or RTCP, if both RTCP and RTP are being
// //    multiplexed over the same destination port).  If the value does not
// //    match any known range then the packet MUST be dropped and an alert
// //    MAY be logged.  This process is summarized in Figure 3.
// //             +----------------+
// //             |        [0..3] -+--> forward to STUN
// //             |                |
// //             |      [16..19] -+--> forward to ZRTP
// //             |                |
// // packet -->  |      [20..63] -+--> forward to DTLS
// //             |                |
// //             |      [64..79] -+--> forward to TURN Channel
// //             |                |
// //             |    [128..191] -+--> forward to RTP/RTCP
namespace mms {
enum UDP_MSG_TYPE {
    UDP_MSG_UNKNOWN = 0,
    UDP_MSG_STUN    = 1,
    UDP_MSG_ZRTP    = 2,
    UDP_MSG_DTLS    = 3,
    UDP_MSG_TURN    = 4,
    UDP_MSG_RTP     = 5,
};

UDP_MSG_TYPE detect_msg_type(uint8_t * data, size_t len);

};