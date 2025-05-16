#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// The tf_flags of tfhd.
// ISO_IEC_14496-12-base-format-2012.pdf, page 68
enum TfhdFlags
{
    // indicates the presence of the base-data-offset field. This provides 
    // An explicit anchor for the data offsets in each track run (see below). If not provided, the base-data-
    // offset for the first track in the movie fragment is the position of the first byte of the enclosing Movie
    // Fragment Box, and for second and subsequent track fragments, the default is the end of the data
    // defined by the preceding fragment. Fragments 'inheriting' their offset in this way must all use
    // The same data-reference (i.e., the data for these tracks must be in the same file).
    TfhdFlagsBaseDataOffset = 0x000001,
    // indicates the presence of this field, which over-rides, in this
    // fragment, the default set up in the Track Extends Box.
    TfhdFlagsSampleDescriptionIndex = 0x000002,
    TfhdFlagsDefaultSampleDuration = 0x000008,
    TfhdFlagsDefautlSampleSize = 0x000010,
    TfhdFlagsDefaultSampleFlags = 0x000020,
    // this indicates that the duration provided in either default-sample-duration,
    // or by the default-duration in the Track Extends Box, is empty, i.e. that there are no samples for this
    // time interval. It is an error to make a presentation that has both edit lists in the Movie Box, and empty-
    // duration fragments.
    TfhdFlagsDurationIsEmpty = 0x010000,
    // if base-data-offset-present is zero, this indicates that the base-data-
    // offset for this track fragment is the position of the first byte of the enclosing Movie Fragment Box.
    // Support for the default-base-is-moof flag is required under the ‘iso5’ brand, and it shall not be used in
    // brands or compatible brands earlier than iso5.
    TfhdFlagsDefaultBaseIsMoof = 0x020000,
};

// 8.8.7 Track Fragment Header Box (tfhd)
// ISO_IEC_14496-12-base-format-2012.pdf, page 68
// Each movie fragment can add zero or more fragments to each track; and a track fragment can add zero or
// more contiguous runs of samples. The track fragment header sets up information and defaults used for those
// runs of samples.

class TfhdBox : public FullBox {
public:
    TfhdBox(uint32_t flags);
    virtual ~TfhdBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    uint32_t track_id_;
// all the following are optional fields
public:
    // The base offset to use when calculating data offsets
    uint64_t base_data_offset_;
    uint32_t sample_description_index_;
    uint32_t default_sample_duration_;
    uint32_t default_sample_size_;
    uint32_t default_sample_flags_;
};

};