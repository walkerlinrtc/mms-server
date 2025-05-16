#pragma once
#include <vector>
#include <memory>

#include "full_box.h"
#include "hdlr.h"
namespace mms {
class NetBuffer;
class SampleEntry;

class StsdBox : public FullBox {
public:
public:
	StsdBox();
    virtual ~StsdBox();

    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<std::shared_ptr<SampleEntry>> entries_;
};
};