#pragma once
#include "box.h"
#include <vector>
#include <string_view>
namespace mms {
class MdatBox : public Box {
public:
    MdatBox();
    virtual ~MdatBox();
public:
    std::vector<std::string_view> datas_;
};
};