#pragma once
#include <memory>
#include "ts_header.hpp"
namespace mms {
class TsPayload;
class TsPacket {
public:
    TsHeader header;
    std::shared_ptr<TsPayload> payload;
};
};