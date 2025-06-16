#pragma once
#include "box.h"
namespace mms {
class TrexBox;
// 8.8.1 Movie Extends Box (mvex)
// ISO_IEC_14496-12-base-format-2012.pdf, page 64
// This box warns readers that there might be Movie Fragment Boxes in this file. To know of all samples in the
// tracks, these Movie Fragment Boxes must be found and scanned in order, and their information logically
// added to that found in the Movie Box.
class MvexBox : public Box {
public:
    MvexBox();
    virtual ~MvexBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    void add_box(std::shared_ptr<Box> box) {
        boxes_.push_back(box);
    }
public:
    std::vector<std::shared_ptr<Box>> find_box(Box::Type type);
protected:
    std::vector<std::shared_ptr<Box>> boxes_;
};
};