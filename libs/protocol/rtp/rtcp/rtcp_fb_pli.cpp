#include "rtcp_fb_pli.h"
using namespace mms;

RtcpFbPli::RtcpFbPli() {
    header_.version = 2;
    header_.pt = PT_PSFB;
    header_.fmt = FMT_PLI;
    header_.length = 2;
}