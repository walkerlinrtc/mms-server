#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
class UrlBox : public FullBox {
public:
    UrlBox();
    virtual ~UrlBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::string location_;
};
};