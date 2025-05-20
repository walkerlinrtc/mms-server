/*
MIT License

Copyright (c) 2021 jiangbaolin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <string>
#include "amf0/amf0_inc.hpp"
#include "rtmp_define.hpp"
#include "flv/flv_define.hpp"
#include "flv/flv_tag.hpp"

namespace mms {
class RtmpMetaDataMessage {
public:
    RtmpMetaDataMessage();
    virtual ~RtmpMetaDataMessage();
public:
    int32_t decode(std::shared_ptr<RtmpMessage> rtmp_msg);
    int32_t decode(std::shared_ptr<FlvTag> rtmp_msg);
    int32_t encode(uint8_t *data, int32_t len);

    bool has_video() {
        return has_video_;
    }

    bool has_audio() {
        return has_audio_;
    }   

    std::shared_ptr<RtmpMessage> msg() {
        return cache_msg_;
    }

    std::shared_ptr<FlvTag> get_flv_tag() {
        return cache_flv_tag_;
    }

    AudioTagHeader::SoundFormat get_audio_codec_id() {
        return audio_codec_id_;
    }

    VideoTagHeader::CodecID get_video_codec_id() {
        return video_codec_id_;
    }

    std::shared_ptr<Amf0Data> get_amf0_data() {
        return amf0_metadata_;
    }
private:
    std::shared_ptr<RtmpMessage> cache_msg_;
    std::shared_ptr<FlvTag> cache_flv_tag_;
    bool has_video_ = false;
    bool has_audio_ = false;

    AudioTagHeader::SoundFormat audio_codec_id_;
    uint32_t audio_channels_;
    uint32_t audio_datarate_;
    uint32_t audio_sample_rate_;
    uint32_t audio_sample_size_;
    uint32_t duration_;
    std::string encoder_;
    uint32_t file_size_;
    uint32_t frame_rate_;
    uint32_t height_;
    uint32_t width_;
    bool stereo_ = true;
    VideoTagHeader::CodecID video_codec_id_;
    uint32_t video_data_rate_;

    bool retrieve_info(Amf0EcmaArray *data);
    bool retrieve_info(Amf0Object *data);

    std::shared_ptr<Amf0Data> amf0_metadata_;
};


};