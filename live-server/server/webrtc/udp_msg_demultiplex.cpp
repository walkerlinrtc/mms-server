#include "udp_msg_demultiplex.hpp"
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

UDP_MSG_TYPE detect_msg_type(uint8_t * data, size_t len)
{
    (void)len;
    if (data[0] <= 3) {
        return UDP_MSG_STUN;
    } else if (data[0] >= 16 && data[0] <= 19) {
        return UDP_MSG_ZRTP;
    } else if (data[0] >= 20 && data[0] <= 63) {
        return UDP_MSG_DTLS;
    } else if (data[0] >= 64 && data[0] <= 79) {
        return UDP_MSG_TURN;
    } else if (data[0] >= 128 && data[0] <= 191) {
        return UDP_MSG_RTP;
    }
    return UDP_MSG_UNKNOWN;
}

};
