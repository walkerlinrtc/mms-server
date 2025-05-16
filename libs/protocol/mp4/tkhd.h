#pragma once
#include <vector>
#include <memory>

#include "full_box.h"
namespace mms {
class NetBuffer;
class TkhdBox : public FullBox {
public:
    TkhdBox(uint8_t version, uint32_t flags) : FullBox(BOX_TYPE_TKHD, version, flags) {}
    virtual ~TkhdBox(){}
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
	uint32_t track_ID_ = 0;
	uint64_t creation_time_ = time(NULL); // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time_; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t duration_ = 0; // default UINT64_MAX(by Movie Header Box timescale)
	//uint32_t reserved;

	//uint32_t reserved2[2];
	int16_t layer_ = 0;
	int16_t alternate_group_ = 0;
	int16_t volume_ = 0; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;	
	int32_t matrix_[9] = {0x00010000,0,0,0,0x00010000,0,0,0,0x40000000}; // u,v,w
	uint32_t width_; // fixed-point 16.16 values
	uint32_t height_; // fixed-point 16.16 values
};
};