#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.4.2 Media Header Box (mdhd)
// ISO_IEC_14496-12-base-format-2012.pdf, page 36
// The media declaration container contains all the objects that declare information about the media data within a
// track.
class MdhdBox : public FullBox {
public:
    MdhdBox();
    virtual ~MdhdBox();
public:
    // The language code for this media. See ISO 639-2/T for the set of three character
    // codes. Each character is packed as the difference between its ASCII value and 0x60. Since the code
    // is confined to being three lower-case letters, these values are strictly positive.
    // @param v The ASCII, for example, 'u'.
    char language0();
    void set_language0(char v);
    // @param v The ASCII, for example, 'n'.
    char language1();
    void set_language1(char v);
    // @param v The ASCII, for example, 'd'.
    char language2();
    void set_language2(char v);
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // An integer that declares the creation time of the presentation (in seconds since
    // midnight, Jan. 1, 1904, in UTC time)
    uint64_t creation_time_;
    // An integer that declares the most recent time the presentation was modified (in
    // seconds since midnight, Jan. 1, 1904, in UTC time)
    uint64_t modification_time_;
    // An integer that specifies the time-scale for the entire presentation; this is the number of
    // time units that pass in one second. For example, a time coordinate system that measures time in
    // sixtieths of a second has a time scale of 60.
    uint32_t timescale_;
    // An integer that declares length of the presentation (in the indicated timescale). This property
    // is derived from the presentation’s tracks: the value of this field corresponds to the duration of the
    // longest track in the presentation. If the duration cannot be determined then duration is set to all 1s.
    uint64_t duration_;
    // The language code for this media. See ISO 639-2/T for the set of three character
    // codes. Each character is packed as the difference between its ASCII value and 0x60. Since the code
    // is confined to being three lower-case letters, these values are strictly positive.
    uint16_t language_;
    uint16_t pre_defined_;
};
};
