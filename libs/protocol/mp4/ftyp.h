#pragma once
#include "box.h"
namespace mms {
class NetBuffer;
class Mp4Builder;
// File format brands
// ISO_IEC_14496-12-base-format-2012.pdf, page 166

#define MP4_BRAND(c1, c2, c3, c4)       \
   (((static_cast<uint32_t>(c1))<<24) |  \
    ((static_cast<uint32_t>(c2))<<16) |  \
    ((static_cast<uint32_t>(c3))<< 8) |  \
    ((static_cast<uint32_t>(c4))))

enum Mp4BoxBrand
{
    Mp4BoxBrandForbidden = 0x00,
    Mp4BoxBrandISOM = MP4_BRAND('i', 's', 'o', 'm'), // 'isom'
    Mp4BoxBrandISO2 = MP4_BRAND('i', 's', 'o', '2'), // 'iso2'
    Mp4BoxBrandAVC1 = MP4_BRAND('a', 'v', 'c', '1'), // 'avc1'
    Mp4BoxBrandMP41 = MP4_BRAND('m', 'p', '4', '1'), // 'mp41'
    Mp4BoxBrandISO5 = MP4_BRAND('i', 's', 'o', '5'), // 'iso5'
    Mp4BoxBrandISO6 = MP4_BRAND('i', 's', 'o', '6'), // 'iso6'
    Mp4BoxBrandMP42 = MP4_BRAND('m', 'p', '4', '2'), // 'mp42'
    Mp4BoxBrandDASH = MP4_BRAND('d', 'a', 's', 'h'), // 'dash'
    Mp4BoxBrandMSDH = MP4_BRAND('m', 's', 'd', 'h'), // 'msdh'
    Mp4BoxBrandMSIX = MP4_BRAND('m', 's', 'i', 'x'), // 'msix'
    Mp4BoxBrandHEV1 = MP4_BRAND('h', 'e', 'v', '1'), // 'hev1'
};

class FtypBox : public Box {
public:
    FtypBox();
    virtual ~FtypBox() = default;
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:    
    uint32_t major_brand_;
    uint32_t minor_version_;
    std::vector<uint32_t> compatible_brands_;
};

class FtypBuilder {
public:
    FtypBuilder(Mp4Builder & builder);
    FtypBuilder & set_major_brand(uint32_t v);
    FtypBuilder & set_minor_version(uint32_t v);
    FtypBuilder & set_compatible_brands(const std::vector<uint32_t> & v);
    virtual ~FtypBuilder();
private:
    Mp4Builder & builder_;
};

};