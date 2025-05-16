#pragma once
#include <string>
#include <vector>
#include <memory>
#include <string_view>

#define BOX_TYPE(c1, c2, c3, c4)       \
   (((static_cast<uint32_t>(c1))<<24) |  \
    ((static_cast<uint32_t>(c2))<<16) |  \
    ((static_cast<uint32_t>(c3))<< 8) |  \
    ((static_cast<uint32_t>(c4))))

namespace mms {
class NetBuffer;
class Box {
public:
    typedef uint32_t Type;
public:
    Box(Type t) : type_(t) {

    }
    virtual ~Box() = default;

    virtual int64_t size();
    int64_t decoded_size();
    virtual void update_size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
    Type type() {
        return type_;
    }
protected:
    size_t size_;
    Type type_;
    int64_t large_size_;
    char user_type_[16];
};

const Box::Type BOX_TYPE_UDTA = BOX_TYPE('u', 'd', 't', 'a');
const Box::Type BOX_TYPE_URL =  BOX_TYPE('u', 'r', 'l', ' ');
const Box::Type BOX_TYPE_URN =  BOX_TYPE('u', 'r', 'n', ' ');
const Box::Type BOX_TYPE_TRAK = BOX_TYPE('t', 'r', 'a', 'k');
const Box::Type BOX_TYPE_TRAF = BOX_TYPE('t', 'r', 'a', 'f');
const Box::Type BOX_TYPE_TKHD = BOX_TYPE('t', 'k', 'h', 'd');
const Box::Type BOX_TYPE_STTS = BOX_TYPE('s', 't', 't', 's');
const Box::Type BOX_TYPE_STSZ = BOX_TYPE('s', 't', 's', 'z');
const Box::Type BOX_TYPE_STZ2 = BOX_TYPE('s', 't', 'z', '2');
const Box::Type BOX_TYPE_STSS = BOX_TYPE('s', 't', 's', 's');
const Box::Type BOX_TYPE_STSD = BOX_TYPE('s', 't', 's', 'd');
const Box::Type BOX_TYPE_STSC = BOX_TYPE('s', 't', 's', 'c');
const Box::Type BOX_TYPE_STCO = BOX_TYPE('s', 't', 'c', 'o');
const Box::Type BOX_TYPE_CO64 = BOX_TYPE('c', 'o', '6', '4');
const Box::Type BOX_TYPE_STBL = BOX_TYPE('s', 't', 'b', 'l');
const Box::Type BOX_TYPE_SINF = BOX_TYPE('s', 'i', 'n', 'f');
const Box::Type BOX_TYPE_SCHI = BOX_TYPE('s', 'c', 'h', 'i');
const Box::Type BOX_TYPE_MVHD = BOX_TYPE('m', 'v', 'h', 'd');
const Box::Type BOX_TYPE_MP4A = BOX_TYPE('m', 'p', '4', 'a');
const Box::Type BOX_TYPE_AVC1 = BOX_TYPE('a', 'v', 'c', '1');
const Box::Type BOX_TYPE_AVC2 = BOX_TYPE('a', 'v', 'c', '2');
const Box::Type BOX_TYPE_AVC3 = BOX_TYPE('a', 'v', 'c', '3');
const Box::Type BOX_TYPE_AVC4 = BOX_TYPE('a', 'v', 'c', '4');
const Box::Type BOX_TYPE_DVAV = BOX_TYPE('d', 'v', 'a', 'v');
const Box::Type BOX_TYPE_DVA1 = BOX_TYPE('d', 'v', 'a', '1');
const Box::Type BOX_TYPE_HEV1 = BOX_TYPE('h', 'e', 'v', '1');
const Box::Type BOX_TYPE_HVC1 = BOX_TYPE('h', 'v', 'c', '1');
const Box::Type BOX_TYPE_DVHE = BOX_TYPE('d', 'v', 'h', 'e');
const Box::Type BOX_TYPE_DVH1 = BOX_TYPE('d', 'v', 'h', '1');
const Box::Type BOX_TYPE_ALAC = BOX_TYPE('a', 'l', 'a', 'c');
const Box::Type BOX_TYPE_MOOV = BOX_TYPE('m', 'o', 'o', 'v');
const Box::Type BOX_TYPE_MOOF = BOX_TYPE('m', 'o', 'o', 'f');
const Box::Type BOX_TYPE_MVEX = BOX_TYPE('m', 'v', 'e', 'x');
const Box::Type BOX_TYPE_MINF = BOX_TYPE('m', 'i', 'n', 'f');
const Box::Type BOX_TYPE_META = BOX_TYPE('m', 'e', 't', 'a');
const Box::Type BOX_TYPE_MDHD = BOX_TYPE('m', 'd', 'h', 'd');
const Box::Type BOX_TYPE_ILST = BOX_TYPE('i', 'l', 's', 't');
const Box::Type BOX_TYPE_HDLR = BOX_TYPE('h', 'd', 'l', 'r');
const Box::Type BOX_TYPE_FTYP = BOX_TYPE('f', 't', 'y', 'p');
const Box::Type BOX_TYPE_ESDS = BOX_TYPE('e', 's', 'd', 's');
const Box::Type BOX_TYPE_EDTS = BOX_TYPE('e', 'd', 't', 's');
const Box::Type BOX_TYPE_DREF = BOX_TYPE('d', 'r', 'e', 'f');
const Box::Type BOX_TYPE_DINF = BOX_TYPE('d', 'i', 'n', 'f');
const Box::Type BOX_TYPE_CTTS = BOX_TYPE('c', 't', 't', 's');
const Box::Type BOX_TYPE_MDIA = BOX_TYPE('m', 'd', 'i', 'a');
const Box::Type BOX_TYPE_VMHD = BOX_TYPE('v', 'm', 'h', 'd');
const Box::Type BOX_TYPE_SMHD = BOX_TYPE('s', 'm', 'h', 'd');
const Box::Type BOX_TYPE_MDAT = BOX_TYPE('m', 'd', 'a', 't');
const Box::Type BOX_TYPE_FREE = BOX_TYPE('f', 'r', 'e', 'e');
const Box::Type BOX_TYPE_HNTI = BOX_TYPE('h', 'n', 't', 'i');
const Box::Type BOX_TYPE_TREF = BOX_TYPE('t', 'r', 'e', 'f');
const Box::Type BOX_TYPE_ODRM = BOX_TYPE('o', 'd', 'r', 'm');
const Box::Type BOX_TYPE_ODKM = BOX_TYPE('o', 'd', 'k', 'm');
const Box::Type BOX_TYPE_MDRI = BOX_TYPE('m', 'd', 'r', 'i');
const Box::Type BOX_TYPE_AVCC = BOX_TYPE('a', 'v', 'c', 'C');
const Box::Type BOX_TYPE_HVCC = BOX_TYPE('h', 'v', 'c', 'C');
const Box::Type BOX_TYPE_HVCE = BOX_TYPE('h', 'v', 'c', 'E');
const Box::Type BOX_TYPE_AVCE = BOX_TYPE('a', 'v', 'c', 'E');
const Box::Type BOX_TYPE_WAVE = BOX_TYPE('w', 'a', 'v', 'e');
const Box::Type BOX_TYPE_WIDE = BOX_TYPE('w', 'i', 'd', 'e');
const Box::Type BOX_TYPE_AC_3 = BOX_TYPE('a', 'c', '-', '3');
const Box::Type BOX_TYPE_EC_3 = BOX_TYPE('e', 'c', '-', '3');
const Box::Type BOX_TYPE_DTSC = BOX_TYPE('d', 't', 's', 'c');
const Box::Type BOX_TYPE_DTSH = BOX_TYPE('d', 't', 's', 'h');
const Box::Type BOX_TYPE_DTSL = BOX_TYPE('d', 't', 's', 'l');
const Box::Type BOX_TYPE_DTSE = BOX_TYPE('d', 't', 's', 'e');
const Box::Type BOX_TYPE_MFRA = BOX_TYPE('m', 'f', 'r', 'a');
const Box::Type BOX_TYPE_MARL = BOX_TYPE('m', 'a', 'r', 'l');
const Box::Type BOX_TYPE_UUID = BOX_TYPE('u', 'u', 'i', 'd');
const Box::Type BOX_TYPE_ELST = BOX_TYPE('e', 'l', 's', 't');
const Box::Type BOX_TYPE_MFHD = BOX_TYPE('m', 'f', 'h', 'd');
const Box::Type BOX_TYPE_SIDX = BOX_TYPE('s', 'i', 'd', 'x');
const Box::Type BOX_TYPE_TFHD = BOX_TYPE('t', 'f', 'h', 'd');
const Box::Type BOX_TYPE_TRUN = BOX_TYPE('t', 'r', 'u', 'n');
const Box::Type BOX_TYPE_STYP = BOX_TYPE('s', 't', 'y', 'p');
const Box::Type BOX_TYPE_TREX = BOX_TYPE('t', 'r', 'e', 'x');
const Box::Type BOX_TYPE_TFDT = BOX_TYPE('t', 'f', 'd', 't');
};