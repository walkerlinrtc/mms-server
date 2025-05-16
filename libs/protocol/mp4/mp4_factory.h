#pragma once
#include <string_view>
#include <memory>
namespace mms {
class Box;
class NetBuffer;

class MP4BoxFactory {
public:
    MP4BoxFactory();
    virtual ~MP4BoxFactory() = default;
public:
    static std::pair<std::shared_ptr<Box>, int64_t> decode_box(NetBuffer & buf);
};
};