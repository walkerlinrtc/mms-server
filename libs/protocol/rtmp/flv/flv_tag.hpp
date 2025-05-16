#pragma once
#include <memory>
#include <string_view>
#include "base/packet.h"
#include "flv_define.hpp"
#include "spdlog/spdlog.h"
namespace mms {
struct TAGDATA {
    virtual ~TAGDATA() {

    }
    
    virtual int32_t decode(const uint8_t *data, size_t len) = 0;
    virtual int32_t encode(uint8_t *data, size_t len) = 0;
    const std::string_view & get_payload() const {
        return payload;
    }
    std::string_view payload;
};

struct AudioTagHeader {
    enum SoundFormat : uint8_t {
        LinearPCM_PE    = 0,    //0 = Linear PCM, platform endian
        ADPCM           = 1,    //1 = ADPCM
        MP3             = 2,    //2 = MP3
        LinearPCM_LE    = 3,    //3 = Linear PCM, little endian
        Nellymoser16kHz = 4,    //4 = Nellymoser 16 kHz mono
        Nellymoser8kHz  = 5,    //5 = Nellymoser 8 kHz mono
        Nellymoser      = 6,    //6 = Nellymoser
        G711ALawPCM     = 7,    //7 = G.711 A-law logarithmic PCM
        G711MuLawPCM    = 8,    //8 = G.711 mu-law logarithmic PCM
        Reserved        = 9,    //9 = reserved
        AAC             = 10,   //10 = AAC
        Speex           = 11,   //11 = Speex
        MP38kHZ         = 12,   //14 = MP3 8 kHz
        DeviceSpecific  = 13,   //15 = Device-specific sound
    };

    enum SoundRate : uint8_t {
        KHZ_5P5         = 0,
        KHZ_11          = 1,
        KHZ_22          = 2,
        KHZ_44          = 3,
    };

    enum SoundSize : uint8_t {
        Sample_8bit      = 0,
        Sample_16bit     = 1,
    };

    enum SoundType : uint8_t {
        MonoSound        = 0,
        StereoSound      = 1,
    };

    enum AACPacketType : uint8_t {
        AACSequenceHeader = 0,
        AACRaw            = 1,  
    };
public:
    enum SoundFormat sound_format:4;
    SoundRate sound_rate:2 = KHZ_44;
    SoundSize sound_size:1 = Sample_16bit;
    SoundType sound_type:1 = StereoSound;
    AACPacketType aac_packet_type = AACSequenceHeader;//IF SoundFormat == 10
public:
    bool is_seq_header();
public:
    int32_t decode(const uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
};

struct AUDIODATA : public TAGDATA {
    AudioTagHeader header;
public:
    int32_t decode(const uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
};

#define MAKE_FOURCC(ch0, ch1, ch2, ch3) (ch3)|(uint32_t(ch2)<<8)|(uint32_t(ch1)<<16)|(uint32_t(ch0)<<24)
struct VideoTagHeader {
    enum FrameType : uint8_t {
        KeyFrame            = 1,
        InterFrame          = 2,
        DisposableFrame     = 3,
        GereratedKeyFrame   = 4,
        VideoInfoFrame      = 5,
    };

    enum CodecID : uint32_t {
        SorensonH264        = 2,
        ScreenVideo         = 3,
        On2VP6              = 4,
        On2VP6_2            = 5,
        ScreenVideo_2       = 6,
        AVC                 = 7,
        HEVC                = 12,
        AV1                 = 13,
        HEVC_FOURCC         = MAKE_FOURCC('h', 'v', 'c', '1'),
        AV1_FOURCC          = MAKE_FOURCC('a', 'v', '0', '1'),
    };
    
    enum AVCPacketType : uint8_t {
        AVCSequenceHeader   = 0,
        AVCNALU             = 1,
        AVCEofSequence      = 2,
    };

    enum ExtPacketType : uint8_t {
        PacketTypeSequeneStart  = 0,
        PacketTypeCodedFrames   = 1,
        PacketTypeSequenceEnd   = 2,
        PacketTypeCodedFramesX  = 3,
        PacketTypeMetadata      = 4,
        PacketTypeMPEG2TSSequenceStart = 5,
    };

    FrameType       frame_type;
    CodecID         codec_id = AVC;
    
    AVCPacketType   avc_packet_type = AVCSequenceHeader;
    uint32_t        composition_time = 0;

    bool            is_extheader = false;
    ExtPacketType   ext_packet_type;
    uint32_t        fourcc;

    int32_t         size_ = -1;//大小
public:
     bool is_key_frame() {
        return frame_type == KeyFrame;
    }

    bool is_seq_header();
    CodecID get_codec_id() {
        return codec_id;
    }

public:
    int32_t decode(const uint8_t *data, size_t len);
    int32_t size();
    int32_t encode(uint8_t *data, size_t len);
    int32_t encode_enhance_header(uint8_t *data, size_t len);
};

struct VIDEODATA : public TAGDATA {
    VideoTagHeader header;
public:
    uint32_t get_pts();
    uint32_t get_dts();
    void set_pts(uint32_t v);
    void set_dts(uint32_t v);

    uint32_t pts;
    uint32_t dts;
public:
    int32_t decode(const uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
};

struct SCRIPTDATA : public TAGDATA {
public:
    int32_t decode(const uint8_t *data, size_t len);
    bool is_seq_header();
    int32_t encode(uint8_t *data, size_t len);
};

struct FlvTag : public Packet {
public:
    FlvTag();
    FlvTag(size_t s);
    ~FlvTag();
public:
    FlvTagHeader & get_tag_header();
    FlvTagHeader::TagType get_tag_type();
    int32_t decode(const uint8_t *data, size_t len);
    int32_t decode_and_store(const uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);
    int32_t encode();
public:
    FlvTagHeader tag_header;
    std::unique_ptr<TAGDATA> tag_data;
};


};