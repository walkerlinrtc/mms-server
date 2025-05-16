#pragma once
#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include "box.h"
#include "base/net_buffer.h"

namespace mms {
class NetBuffer;
class FullBox : public Box {
public:
    FullBox(Type t, uint8_t version, uint32_t flags);
    virtual ~FullBox() = default;

    virtual int64_t size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
    Type type() {
        return type_;
    }
public:
    uint8_t version_;
    uint32_t flags_ = 0;
};

};