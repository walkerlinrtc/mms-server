#include "rtmp_metadata_message.hpp"

using namespace mms;
RtmpMetaDataMessage::RtmpMetaDataMessage() {

}

RtmpMetaDataMessage::~RtmpMetaDataMessage() {

}

int32_t RtmpMetaDataMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    int32_t consumed = 0;
    int32_t pos = 0;
    auto using_data = rtmp_msg->get_using_data();
    const uint8_t *payload = (const uint8_t*)using_data.data();
    int32_t len = using_data.size();
    Amf0String name;
    Amf0EcmaArray metadata_arr;
    Amf0Object medata_obj;
    consumed = name.decode(payload, len);
    if (consumed < 0) {
        return -1;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    if (name.get_value() == "@setDataFrame") {
        Amf0String name2;
        consumed = name2.decode(payload, len);
        if (consumed < 0) {
            return -2;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
    }

    if (len < 1) {
        return -3;
    }
    auto marker = payload[0];
    if (marker == OBJECT_MARKER) {
        amf0_metadata_ = std::make_shared<Amf0Object>();
        std::shared_ptr<Amf0Object> metadata_ = std::static_pointer_cast<Amf0Object>(amf0_metadata_);
        consumed = metadata_->decode(payload, len);
        if (consumed < 0) {
            return -3;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
        
        cache_msg_ = rtmp_msg;
        {// get metadata info
            auto t = metadata_->get_property<Amf0Number>("audiocodecid");
            if (t) {
                audio_codec_id_ = (AudioTagHeader::SoundFormat)*t;
                has_audio_ = true;
            }

            t = metadata_->get_property<Amf0Number>("audiochannels");
            if (t) {
                audio_channels_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiodatarate");
            if (t) {
                audio_datarate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiosamplerate");
            if (t) {
                audio_sample_rate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiosamplesize");
            if (t) {
                audio_sample_size_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("duration");
            if (t) {
                duration_ = *t;
            }

            auto m = metadata_->get_property<Amf0String>("encoder");
            if (m) {
                encoder_ = *m;
            }

            t = metadata_->get_property<Amf0Number>("fileSize");
            if (t) {
                file_size_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("framerate");
            if (t) {
                frame_rate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("height");
            if (t) {
                height_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("width");
            if (t) {
                width_ = *t;
            }
            
            t = metadata_->get_property<Amf0Boolean>("stereo");
            if (t) {
                stereo_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("videocodecid");
            if (t) {
                video_codec_id_ = (VideoTagHeader::CodecID)*t;
                has_video_ = true;
            }

            t = metadata_->get_property<Amf0Number>("videodatarate");
            if (t) {
                video_data_rate_ = *t;
            }        
        }
    } else if (marker == ECMA_ARRAY_MARKER) {
        amf0_metadata_ = std::make_shared<Amf0EcmaArray>();
        std::shared_ptr<Amf0EcmaArray> metadata_ = std::static_pointer_cast<Amf0EcmaArray>(amf0_metadata_);
        consumed = metadata_->decode(payload, len);
        if (consumed < 0) {
            return -3;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
        
        cache_msg_ = rtmp_msg;
        {// get metadata info
            auto t = metadata_->get_property<Amf0Number>("audiocodecid");
            if (t) {
                audio_codec_id_ = (AudioTagHeader::SoundFormat)*t;
                has_audio_ = true;
            }

            t = metadata_->get_property<Amf0Number>("audiochannels");
            if (t) {
                audio_channels_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiodatarate");
            if (t) {
                audio_datarate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiosamplerate");
            if (t) {
                audio_sample_rate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("audiosamplesize");
            if (t) {
                audio_sample_size_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("duration");
            if (t) {
                duration_ = *t;
            }

            auto m = metadata_->get_property<Amf0String>("encoder");
            if (m) {
                encoder_ = *m;
            }

            t = metadata_->get_property<Amf0Number>("fileSize");
            if (t) {
                file_size_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("framerate");
            if (t) {
                frame_rate_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("height");
            if (t) {
                height_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("width");
            if (t) {
                width_ = *t;
            }
            
            t = metadata_->get_property<Amf0Boolean>("stereo");
            if (t) {
                stereo_ = *t;
            }

            t = metadata_->get_property<Amf0Number>("videocodecid");
            if (t) {
                video_codec_id_ = (VideoTagHeader::CodecID)*t;
                has_video_ = true;
            }

            t = metadata_->get_property<Amf0Number>("videodatarate");
            if (t) {
                video_data_rate_ = *t;
            }        
        }
    } else {
        return -5;
    }
    
    return pos;
}

int32_t RtmpMetaDataMessage::decode(std::shared_ptr<FlvTag> flv_tag) {
    int32_t consumed = 0;
    int32_t pos = 0;
    SCRIPTDATA *script_data = (SCRIPTDATA *)flv_tag->tag_data.get();
    std::string_view payload_vw = script_data->get_payload();
    const uint8_t *payload = (const uint8_t*)payload_vw.data();
    int32_t len = payload_vw.size();
    Amf0String name;
    std::unique_ptr<Amf0Data> metadata;
    // Amf0EcmaArray metadata;
    consumed = name.decode(payload, len);
    if (consumed < 0) {
        return -1;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    if (name.get_value() == "@setDataFrame") {
        Amf0String name2;
        consumed = name2.decode(payload, len);
        if (consumed < 0) {
            return -2;
        }
        pos += consumed;
        payload += consumed;
        len -= consumed;
    }

    if (len < 1) {
        return -3;
    }

    AMF0_MARKER_TYPE marker = (AMF0_MARKER_TYPE)(*payload);
    if (marker == OBJECT_MARKER) {
        metadata = std::make_unique<Amf0Object>();
    } else if (marker == ECMA_ARRAY_MARKER) {
        metadata = std::make_unique<Amf0EcmaArray>();
    }

    consumed = metadata->decode(payload, len);
    if (consumed < 0) {
        return -4;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    
    cache_flv_tag_ = flv_tag;
    if (marker == OBJECT_MARKER) {
        retrieve_info((Amf0Object*)metadata.get());
    } else if (marker == ECMA_ARRAY_MARKER) {
        retrieve_info((Amf0EcmaArray*)metadata.get());
    }
    
    return pos;
}

bool RtmpMetaDataMessage::retrieve_info(Amf0EcmaArray *metadata) {
    auto t = metadata->get_property<Amf0Number>("audiocodecid");
    if (t) {
        audio_codec_id_ = (AudioTagHeader::SoundFormat)*t;
        has_audio_ = true;
    }

    t = metadata->get_property<Amf0Number>("audiochannels");
    if (t) {
        audio_channels_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiodatarate");
    if (t) {
        audio_datarate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiosamplerate");
    if (t) {
        audio_sample_rate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiosamplesize");
    if (t) {
        audio_sample_size_ = *t;
    }

    t = metadata->get_property<Amf0Number>("duration");
    if (t) {
        duration_ = *t;
    }

    auto m = metadata->get_property<Amf0String>("encoder");
    if (m) {
        encoder_ = *m;
    }

    t = metadata->get_property<Amf0Number>("fileSize");
    if (t) {
        file_size_ = *t;
    }

    t = metadata->get_property<Amf0Number>("framerate");
    if (t) {
        frame_rate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("height");
    if (t) {
        height_ = *t;
    }

    t = metadata->get_property<Amf0Number>("width");
    if (t) {
        width_ = *t;
    }
    
    auto v = metadata->get_property<Amf0Boolean>("stereo");
    if (v) {
        stereo_ = *v;
    }

    t = metadata->get_property<Amf0Number>("videocodecid");
    if (t) {
        video_codec_id_ = (VideoTagHeader::CodecID)*t;
        has_video_ = true;
    }

    t = metadata->get_property<Amf0Number>("videodatarate");
    if (t) {
        video_data_rate_ = *t;
    }   
    return true;     
}

bool RtmpMetaDataMessage::retrieve_info(Amf0Object *metadata) {
    auto t = metadata->get_property<Amf0Number>("audiocodecid");
    if (t) {
        audio_codec_id_ = (AudioTagHeader::SoundFormat)*t;
        has_audio_ = true;
    }

    t = metadata->get_property<Amf0Number>("audiochannels");
    if (t) {
        audio_channels_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiodatarate");
    if (t) {
        audio_datarate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiosamplerate");
    if (t) {
        audio_sample_rate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("audiosamplesize");
    if (t) {
        audio_sample_size_ = *t;
    }

    t = metadata->get_property<Amf0Number>("duration");
    if (t) {
        duration_ = *t;
    }

    auto m = metadata->get_property<Amf0String>("encoder");
    if (m) {
        encoder_ = *m;
    }

    t = metadata->get_property<Amf0Number>("fileSize");
    if (t) {
        file_size_ = *t;
    }

    t = metadata->get_property<Amf0Number>("framerate");
    if (t) {
        frame_rate_ = *t;
    }

    t = metadata->get_property<Amf0Number>("height");
    if (t) {
        height_ = *t;
    }

    t = metadata->get_property<Amf0Number>("width");
    if (t) {
        width_ = *t;
    }
    
    auto v = metadata->get_property<Amf0Boolean>("stereo");
    if (v) {
        stereo_ = *v;
    }

    t = metadata->get_property<Amf0Number>("videocodecid");
    if (t) {
        video_codec_id_ = (VideoTagHeader::CodecID)*t;
        has_video_ = true;
    }

    t = metadata->get_property<Amf0Number>("videodatarate");
    if (t) {
        video_data_rate_ = *t;
    }  
    return true;
}

int32_t RtmpMetaDataMessage::encode(uint8_t *data, int32_t len) {
    //todo implement this method
    uint8_t *data_start = data;
    Amf0String name;
    name.set_value("@setDataFrame");
    int32_t consumed = name.encode(data, len);
    if (consumed <= 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;

    Amf0String name2;
    name2.set_value("onMetaData");
    consumed = name2.encode(data, len);
    if (consumed <= 0) {
        return 0;
    }
    data += consumed;
    len -= consumed;

    consumed = amf0_metadata_->encode(data, len);
    if (consumed <= 0) {
        return 0;
    }
    data += consumed;
    len -= consumed;
 
    return data - data_start;
}
