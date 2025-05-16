#pragma once
#include <vector>
#include <memory>

#include "box.h"

namespace mms {
class TkhdBox;
class MdiaBox;
class NetBuffer;

// 8.3.1 Track Box (trak)
// ISO_IEC_14496-12-base-format-2012.pdf, page 32
// This is a container box for a single track of a presentation. A presentation consists of one or more tracks.
// Each track is independent of the other tracks in the presentation and carries its own temporal and spatial
// information. Each track will contain its associated Media Box.
class TrakBox : public Box {
public:
    TrakBox();
    virtual ~TrakBox() = default;
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    void set_tkhd(std::shared_ptr<TkhdBox> tkhd);
    std::shared_ptr<TkhdBox> get_tkhd();
    void set_mdia(std::shared_ptr<MdiaBox> mdia);
    std::shared_ptr<MdiaBox> get_mdia();
protected:
    std::shared_ptr<TkhdBox> tkhd_;
    std::shared_ptr<MdiaBox> mdia_;
};
};