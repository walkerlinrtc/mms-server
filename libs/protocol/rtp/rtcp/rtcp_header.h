#pragma once
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <memory>
#include <vector>
// Name   | Value | Brief Description
// ----------+-------+------------------------------------
// RTPFB  |  205  | Transport layer FB message
// PSFB   |  206  | Payload-specific FB message

#define PT_RTPFB    205
#define PT_PSFB     206
#define PT_RTCP_SR  200
#define PT_RTCP_RR  201

// rfc4585 6.3.  Payload-Specific Feedback Messages

//    Payload-Specific FB messages are identified by the value PT=PSFB as
//    RTCP message type.

//    Three payload-specific FB messages are defined so far plus an
//    application layer FB message.  They are identified by means of the
//    FMT parameter as follows:

//       0:     unassigned
//       1:     Picture Loss Indication (PLI)
//       2:     Slice Loss Indication (SLI)
//       3:     Reference Picture Selection Indication (RPSI)
//       4-14:  unassigned
//       15:    Application layer FB (AFB) message
//       16-30: unassigned
//       31:    reserved for future expansion of the sequence number space
#define FMT_PLI     0x01
#define FMT_SLI     0x02
#define FMT_AFB     0x0F
#define FMT_RSV     0x1E

namespace mms
{
    class RtcpHeader
    {
    public:
        static bool is_rtcp_packet(uint8_t *data, size_t len);
        static uint8_t parse_pt(uint8_t *data, size_t len);
        RtcpHeader();

        uint8_t padding = 0;    // 1bit
        uint8_t version = 2;    // 2bit
        uint8_t rc = 0;        // 5bit
        uint8_t pt;
        uint16_t length;
        uint32_t sender_ssrc;
    public:
        int32_t encode(uint8_t *data, size_t len);
        int32_t decode(uint8_t *data, size_t len);
        size_t size() {
            return 12;
        }
        uint8_t get_pt() {
            return pt;
        }

        uint8_t get_rc() {
            return rc;
        }
    };

};