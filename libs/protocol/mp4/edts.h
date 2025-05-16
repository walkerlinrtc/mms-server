#pragma once
#include "box.h"
namespace mms {
class ElstBox;

class EdtsBox : public Box {
public:
    EdtsBox();
    virtual ~EdtsBox();
    void set_elst(std::shared_ptr<ElstBox> elst);
    std::shared_ptr<ElstBox> get_elst();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
protected:
    std::shared_ptr<ElstBox> elst_;
};
};