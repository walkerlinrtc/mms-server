#pragma once
#include <stdint.h>
namespace mms {
// The payload of ts packet, can be PES or PSI payload.
class TsPayload {
public:
    TsPayload() {};
    virtual ~TsPayload() {};
public:
    virtual int32_t decode(uint8_t *data, int32_t len) = 0;
public:
    virtual int32_t size() = 0;
    virtual int32_t encode(uint8_t *data, int32_t len) = 0;
};
};