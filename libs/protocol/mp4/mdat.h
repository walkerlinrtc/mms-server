#pragma once
#include "box.h"
#include <vector>
#include <string_view>
namespace mms {
class NetBuffer;

class MdatBox : public Box {
public:
    MdatBox();
    virtual ~MdatBox();
    // int64_t size();
    // int64_t encode(NetBuffer & buf);
public:
    std::vector<std::string_view> datas_;
};
};