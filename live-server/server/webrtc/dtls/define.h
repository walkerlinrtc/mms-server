#pragma once
#include <stdint.h>
namespace mms {
typedef uint16_t SRTPProtectionProfile;
#define SRTP_AES128_CM_HMAC_SHA1_80 0x0001  //我们默认使用这种先
};