#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.7.4 Sample To Chunk Box (stsc), for Audio/Video.
// ISO_IEC_14496-12-base-format-2012.pdf, page 58
class StscEntry {
public:
    // An integer that gives the index of the first chunk in this run of chunks that share the
    // same samples-per-chunk and sample-description-index; the index of the first chunk in a track has the
    // value 1 (the first_chunk field in the first record of this box has the value 1, identifying that the first
    // sample maps to the first chunk).
    uint32_t first_chunk;
    // An integer that gives the number of samples in each of these chunks
    uint32_t samples_per_chunk;
    // An integer that gives the index of the sample entry that describes the
    // samples in this chunk. The index ranges from 1 to the number of sample entries in the Sample
    // Description Box
    uint32_t sample_description_index;
    int64_t size() {
        return 12;
    }
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

// 8.7.4 Sample To Chunk Box (stsc), for Audio/Video.
// ISO_IEC_14496-12-base-format-2012.pdf, page 58
// Samples within the media data are grouped into chunks. Chunks can be of different sizes, and the samples
// within a chunk can have different sizes. This table can be used to find the chunk that contains a sample,
// its position, and the associated sample description.
class StscBox : public FullBox {
public:
    StscBox();
    virtual ~StscBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<StscEntry> entries_;
};
};