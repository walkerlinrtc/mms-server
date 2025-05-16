#pragma once
#include "box.h"
namespace mms {
class StblBox : public Box {
public:
    StblBox();
    virtual ~StblBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    void add_box(std::shared_ptr<Box> child);
    std::shared_ptr<Box> find_box(Box::Type type);
protected:
    std::vector<std::shared_ptr<Box>> boxes_;
};
};