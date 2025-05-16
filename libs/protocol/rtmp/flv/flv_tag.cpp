#include "flv_tag.hpp"
#include "spdlog/spdlog.h"
using namespace mms;



bool AudioTagHeader::is_seq_header() {
    return aac_packet_type == AACSequenceHeader;
}

int32_t AudioTagHeader::decode(const uint8_t *data, size_t len) {
    const uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    sound_type = (SoundType)((*data) & 0x01);
    sound_size = (SoundSize)(((*data) >> 1) & 0x01);
    sound_rate = (SoundRate)(((*data) >> 2) & 0x03);
    sound_format = (SoundFormat)((*data >> 4) & 0x0f);

    len--;
    data++;

    if (sound_format == AAC) {
        if (len < 1) {
            return -2;
        }
        aac_packet_type = (AACPacketType)(*data); 
        len--;
        data++;
    }
    return data - data_start;
}

int32_t AudioTagHeader::encode(uint8_t *data, size_t len) {
    auto data_start = data;
    if (len < 1) {
        return -1;
    }
    data[0] = (sound_format << 4) | (sound_rate << 2) | (sound_size << 1) | sound_type;
    len--;
    data++;

    if (sound_format == AAC) {
        if (len < 1) {
            return -2;
        }
        data[0] = aac_packet_type;
        data++;
        len--;
    }

    return data - data_start;
}

bool VideoTagHeader::is_seq_header() {
    if (!is_extheader) {
        if (codec_id == AVC || codec_id == HEVC || codec_id == HEVC_FOURCC) {
            return avc_packet_type == AVCSequenceHeader;
        } else {
            return false;
        }
    } else {
        if (codec_id == HEVC || codec_id == HEVC_FOURCC || fourcc == HEVC_FOURCC) {
            return ext_packet_type == PacketTypeSequeneStart;
        } else {
            return false;
        }
    }
    
    return false;
}

int32_t VideoTagHeader::decode(const uint8_t *data, size_t len) { 
    auto data_start = data;
    if (len < 1) {
        return 0;
    }

    frame_type = (FrameType)((*data>>4)&0x0f);
    if (frame_type & 0x08) { 
        is_extheader = true;
    }
    frame_type = (FrameType)((*data>>4)&0x07); 
    
    if (is_extheader) {
        ext_packet_type = (ExtPacketType)((*data)&0x0f);
        data++;
        len--;
        if (len < 4) {
            return 0;
        }

        fourcc = ntohl(*(uint32_t*)data);
        data += 4;
        len -= 4;
        if (fourcc == HEVC_FOURCC) {
            codec_id = HEVC;
            if (ext_packet_type == PacketTypeSequeneStart) {
                avc_packet_type = AVCSequenceHeader;
            } else if (ext_packet_type == PacketTypeCodedFrames || ext_packet_type == PacketTypeCodedFramesX) {
                if (ext_packet_type == PacketTypeCodedFrames) {
                    if (len < 3) {
                        return 0;
                    }
                    composition_time = 0;
                    uint8_t *p = (uint8_t*)&composition_time;
                    p[0] = data[2];
                    p[1] = data[1];
                    p[2] = data[0];
                    data += 3;
                    len -= 3;
                }
                
                avc_packet_type = AVCNALU;
            } else if (ext_packet_type == PacketTypeSequenceEnd) {
                avc_packet_type = AVCEofSequence;
            }
        } else {
            return -1;
        }
    } else {
        codec_id = CodecID(((*data))&0x0f);
        len--;
        data++;

        if (codec_id == AVC || codec_id == HEVC) {
            if (len < 4) {
                return 0;
            }
            avc_packet_type = (AVCPacketType)(*data);
            data++;
            len--;

            composition_time = 0;
            uint8_t *p = (uint8_t*)&composition_time;
            p[0] = data[2];
            p[1] = data[1];
            p[2] = data[0];
            data += 3;
            len -= 3;
        } else {
            return -1;
        }
    }
    
    size_ = data - data_start;
    return data - data_start;
}

int32_t VideoTagHeader::encode(uint8_t *data, size_t len) {
    auto data_start = data;
    if (!is_extheader) {
        if (len < 1) {
            return 0;
        }

        data[0] = (frame_type << 4) | codec_id;
        len--;
        data++;

        if (codec_id == HEVC_FOURCC) {
            codec_id = HEVC;
        }

        if (codec_id == AVC || codec_id == HEVC) {
            if (len < 4) {
                return -2;
            }
            *data = avc_packet_type;
            data++;
            len--;

            uint8_t *p = (uint8_t*)&composition_time;
            data[0] = p[2];
            data[1] = p[1];
            data[2] = p[0];
            data += 3;
            len -= 3;
        } else {
            return -1;
        }
    } else {
        if (len < 1) {
            return 0;
        }
        data[0] = 0x80 | ((frame_type & 0x0f) << 4) | (ext_packet_type & 0x0f);
        len--;
        data++;

        if (len < 4) {
            return 0;
        }

        *(uint32_t*)data = htonl(fourcc);
        data += 4;
        len -= 4;
        if (fourcc == HEVC_FOURCC) {
            if (ext_packet_type == PacketTypeCodedFrames) {
                uint8_t *p = (uint8_t*)&composition_time;
                if (len < 3) {
                    return 0;
                }
                data[0] = p[2];
                data[1] = p[1];
                data[2] = p[0];
                data += 3;
                len -= 3;
            }
        }
    }
    
    return data - data_start;
}

int32_t VideoTagHeader::size() {
    int32_t s = 0;
    if (!is_extheader) {
        s = 1;
        if (codec_id == AVC || codec_id == HEVC) {
            s += 4;
        }
    } else {
        s = 5;
        if (fourcc == HEVC_FOURCC) {
            if (ext_packet_type == PacketTypeCodedFrames) {
                s += 3;
            }
        }
    }
    return s;
}

int32_t VideoTagHeader::encode_enhance_header(uint8_t *data, size_t len) {
    ((void)data);
    ((void)len);
    return 0;
}



int32_t AUDIODATA::decode(const uint8_t *data, size_t len) {
    auto data_start = data;
    auto consumed = header.decode(data, len);
    if (consumed < 0) {
        return -1;
    }
    data += consumed;
    len -= consumed;
    payload = std::string_view((char*)data, len);
    data += len;
    return data - data_start;
}

int32_t AUDIODATA::encode(uint8_t *data, size_t len) {
    auto data_start = data;
    auto consumed = header.encode(data, len);
    if (consumed < 0) {
        return -1;
    }
    data += consumed;
    len -= consumed;
    if (len < payload.size()) {
        return -2;
    }

    if (payload.data() == (const char*)data) {
        data += payload.size();
        return data - data_start;
    }
    memcpy(data, payload.data(), payload.size());
    payload = std::string_view((char*)data, payload.size());
    data += payload.size();
    return data - data_start;
}



uint32_t VIDEODATA::get_pts() {
    return pts;
}

uint32_t VIDEODATA::get_dts() {
    return dts;
}

void VIDEODATA::set_pts(uint32_t v) {
    pts = v;
}

void VIDEODATA::set_dts(uint32_t v) {
    dts = v;
}

int32_t VIDEODATA::decode(const uint8_t *data, size_t len) {
    auto data_start = data;
    auto consumed = header.decode(data, len);
    if (consumed <= 0) {
        return consumed;
    }
    data += consumed;
    len -= consumed;
    payload = std::string_view((char*)data, len);
    data += len;
    return data - data_start;
}

int32_t VIDEODATA::encode(uint8_t *data, size_t len) {
    auto data_start = data;
    auto consumed = header.encode(data, len);
    if (consumed < 0) {
        return -1;
    }
    
    data += consumed;
    len -= consumed;
    if (len < payload.size()) {
        return -2;
    }

    if (payload.data() == (const char*)data) {
        data += payload.size();
        return data - data_start;
    }

    memcpy(data, payload.data(), payload.size());
    payload = std::string_view((char*)data, payload.size());
    data += payload.size();
    return data - data_start;
}

int32_t SCRIPTDATA::decode(const uint8_t *data, size_t len) {
    payload = std::string_view((char*)data, len);
    return len;
}

bool SCRIPTDATA::is_seq_header() {
    return false;
}

int32_t SCRIPTDATA::encode(uint8_t *data, size_t len) {
    if (len < payload.size()) {
        return -1;
    }
    memcpy(data, payload.data(), payload.size());
    payload = std::string_view((char*)data, payload.size());
    return payload.size();
}



FlvTag::FlvTag() {

}

FlvTag::FlvTag(size_t s) : Packet(PACKET_FLV, s) {

}

FlvTag::~FlvTag() {
    tag_data.reset();
}

FlvTagHeader & FlvTag::get_tag_header() {
    return tag_header;
}  

FlvTagHeader::TagType FlvTag::get_tag_type() {
    return (FlvTagHeader::TagType)(tag_header.tag_type & 0x1f);
}  

int32_t FlvTag::decode(const uint8_t *data, size_t len) {
    auto data_start = data;
    auto consumed = tag_header.decode(data, len);
    if (consumed < 0) {
        return -1;
    } else if (consumed == 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;

    if (len < tag_header.data_size) {
        return 0;
    }

    if ((tag_header.tag_type & 0x1f) == FlvTagHeader::AudioTag) {
        tag_data = std::make_unique<AUDIODATA>();
        consumed = tag_data->decode(data, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }
    } else if ((tag_header.tag_type & 0x1f) == FlvTagHeader::VideoTag) {
        tag_data = std::make_unique<VIDEODATA>();
        consumed = tag_data->decode(data, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }

        if (consumed > 0) {
            VIDEODATA *video_data = (VIDEODATA *)tag_data.get();
            if (video_data->header.codec_id == VideoTagHeader::AVC) {
                video_data->set_pts(tag_header.timestamp + video_data->header.composition_time);
                video_data->set_dts(tag_header.timestamp);
            }
        }
    } else if ((tag_header.tag_type & 0x1f) == FlvTagHeader::ScriptTag) {
        tag_data = std::make_unique<SCRIPTDATA>();
        consumed = tag_data->decode(data, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }
    } else {
        return -3;
    }

    if (consumed == 0) {
        return 0;
    }
    data += consumed;
    return data - data_start;
}

int32_t FlvTag::decode_and_store(const uint8_t *data, size_t len) {
    auto data_start = data;
    auto tag_header_consumed = tag_header.decode(data, len);
    if (tag_header_consumed < 0) {
        return -1;
    } else if (tag_header_consumed == 0) {
        return 0;
    }

    data += tag_header_consumed;
    len -= tag_header_consumed;

    if (len < tag_header.data_size) {
        return 0;
    }

    auto total_bytes = tag_header_consumed + tag_header.data_size;
    // 将数据存储起来
    alloc_buf(total_bytes);
    memcpy(data_buf_, data_start, total_bytes);
    // 解析TAGDATA字段
    if ((tag_header.tag_type & 0x1f) == FlvTagHeader::AudioTag) {
        tag_data = std::make_unique<AUDIODATA>();
        // auto consumed = tag_data->decode(raw_data_buf.get() + tag_header_consumed, tag_header.data_size);
        auto consumed = tag_data->decode(data_buf_ + tag_header_consumed, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }
    } else if ((tag_header.tag_type & 0x1f) == FlvTagHeader::VideoTag) {
        tag_data = std::make_unique<VIDEODATA>();
        auto consumed = tag_data->decode(data_buf_ + tag_header_consumed, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }

        if (consumed > 0) {
            VIDEODATA *video_data = (VIDEODATA *)tag_data.get();
            if (video_data->header.codec_id == VideoTagHeader::AVC) {
                video_data->set_pts(tag_header.timestamp + video_data->header.composition_time);
                video_data->set_dts(tag_header.timestamp);
            }
        }
    } else if ((tag_header.tag_type & 0x1f) == FlvTagHeader::ScriptTag) {
        tag_data = std::make_unique<SCRIPTDATA>();
        auto consumed = tag_data->decode(data_buf_ + tag_header_consumed, tag_header.data_size);
        if (consumed < 0) {
            return -2;
        }
    } else {
        return -3;
    }

    inc_used_bytes(total_bytes);

    return total_bytes;
}

int32_t FlvTag::encode() {
    auto data = data_buf_;
    int32_t len = data_bytes_;
    int32_t consumed = tag_header.encode(data, len);
    if (consumed < 0) {
        return -1;
    }

    data += consumed;
    len -= consumed;
    consumed = tag_data->encode(data, len);
    if (consumed < 0) {
        return -2;
    }
    
    data += consumed;
    inc_used_bytes(data - data_buf_);
    return data - data_buf_;
}