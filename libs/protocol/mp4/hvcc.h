#pragma once
#include <vector>
#include <memory>

#include "box.h"

namespace mms {
class NetBuffer;
class HvccBox : public Box {
public:
    HvccBox();
    virtual ~HvccBox() = default;

    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
protected:
    std::string hevc_config_;
};
};