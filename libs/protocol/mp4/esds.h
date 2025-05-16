#pragma once
#include <stdint.h>
#include "full_box.h"

namespace mms {
class NetBuffer;
// Table 1 — List of Class Tags for Descriptors
// ISO_IEC_14496-1-System-2010.pdf, page 31
enum Mp4ESTagEs {
    Mp4ESTagESforbidden = 0x00,
    Mp4ESTagESObjectDescrTag = 0x01,
    Mp4ESTagESInitialObjectDescrTag = 0x02,
    Mp4ESTagESDescrTag = 0x03,
    Mp4ESTagESDecoderConfigDescrTag = 0x04,
    Mp4ESTagESDecSpecificInfoTag = 0x05,
    Mp4ESTagESSLConfigDescrTag = 0x06,
    Mp4ESTagESExtSLConfigDescrTag = 0x064,
};

// Table 5 — objectTypeIndication Values
// ISO_IEC_14496-1-System-2010.pdf, page 49
enum Mp4ObjectType
{
    Mp4ObjectTypeForbidden = 0x00,
    // Audio ISO/IEC 14496-3
    Mp4ObjectTypeAac = 0x40,
    // Audio ISO/IEC 13818-3
    Mp4ObjectTypeMp3 = 0x69,
    // Audio ISO/IEC 11172-3
    Mp4ObjectTypeMp1a = 0x6B,
};

// Table 6 — streamType Values
// ISO_IEC_14496-1-System-2010.pdf, page 51
enum Mp4StreamType
{
    Mp4StreamTypeForbidden = 0x00,
    Mp4StreamTypeAudioStream = 0x05,
};


// 7.2.2.2 BaseDescriptor
// ISO_IEC_14496-1-System-2010.pdf, page 32
class Mp4BaseDescriptor
{
public:
    // The values of the class tags are
    // defined in Table 2. As an expandable class the size of each class instance in bytes is encoded and accessible
    // through the instance variable sizeOfInstance (see 8.3.3).
    Mp4ESTagEs tag_; // bit(8)
    // The decoded or encoded variant length.
    int32_t vlen_; // bit(28)
public:
    Mp4BaseDescriptor();
    virtual ~Mp4BaseDescriptor();
public:
    virtual int64_t size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
};

// 7.3.2.3 SL Packet Header Configuration
// ISO_IEC_14496-1-System-2010.pdf, page 92
class Mp4SLConfigDescriptor : public Mp4BaseDescriptor
{
public:
    uint8_t predefined_;
public:
    Mp4SLConfigDescriptor();
    virtual ~Mp4SLConfigDescriptor();
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

// 7.2.6.7 DecoderSpecificInfo
// ISO_IEC_14496-1-System-2010.pdf, page 51
class Mp4DecoderSpecificInfo : public Mp4BaseDescriptor
{
public:
    // AAC Audio Specific Config.
    // 1.6.2.1 AudioSpecificConfig, in ISO_IEC_14496-3-AAC-2001.pdf, page 33.
    std::string asc_;
public:
    Mp4DecoderSpecificInfo();
    virtual ~Mp4DecoderSpecificInfo();
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

// 7.2.6.6 DecoderConfigDescriptor
// ISO_IEC_14496-1-System-2010.pdf, page 48
class Mp4DecoderConfigDescriptor : public Mp4BaseDescriptor
{
public:
    // An indication of the object or scene description type that needs to be supported
    // by the decoder for this elementary stream as per Table 5.
    Mp4ObjectType object_type_indication_; // bit(8)
    Mp4StreamType stream_type_; // bit(6)
    uint8_t upstream_; // bit(1)
    uint8_t reserved_; // bit(1)
    uint32_t buffer_size_db_; // bit(24)
    uint32_t max_bitrate_;
    uint32_t avg_bitrate_;
    std::shared_ptr<Mp4DecoderSpecificInfo> dec_specific_info_; // optional.
public:
    Mp4DecoderConfigDescriptor();
    virtual ~Mp4DecoderConfigDescriptor();
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

// 7.2.6.5 ES_Descriptor
// ISO_IEC_14496-1-System-2010.pdf, page 47
class Mp4ES_Descriptor : public Mp4BaseDescriptor
{
public:
    uint16_t ES_ID_;
    uint8_t streamDependenceFlag_; // bit(1)
    uint8_t URL_Flag_; // bit(1)
    uint8_t OCRstreamFlag_; // bit(1)
    uint8_t streamPriority_; // bit(5)
    // if (streamDependenceFlag)
    uint16_t dependsOn_ES_ID_;
    // if (URL_Flag)
    std::string URLstring_;
    // if (OCRstreamFlag)
    uint16_t OCR_ES_Id_;
    Mp4DecoderConfigDescriptor decConfigDescr_;
    Mp4SLConfigDescriptor slConfigDescr_;
public:
    Mp4ES_Descriptor();
    virtual ~Mp4ES_Descriptor();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
};

// 5.6 Sample Description Boxes
// Elementary Stream Descriptors (esds)
// ISO_IEC_14496-14-MP4-2003.pdf, page 15
// @see http://www.mp4ra.org/codecs.html
class EsdsBox : public FullBox {
public:
    EsdsBox();
    virtual ~EsdsBox();
public:
    Mp4ES_Descriptor es_;
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};
}