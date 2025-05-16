/*
 * @Author: jbl19860422
 * @Date: 2023-12-25 22:23:38
 * @LastEditTime: 2023-12-25 23:21:58
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\codec\codec.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
#include "json/json.h"

namespace mms {
enum CodecType {
    CODEC_H264  = 0,
    CODEC_HEVC  = 1,
    CODEC_AAC   = 2,
    CODEC_MP3   = 3,
    CODEC_AV1   = 4,
    CODEC_OPUS  = 5,
    CODEC_G711_ALOW = 6,
    CODEC_G711_ULOW = 7,
};

class Codec {
public:
    Codec(CodecType type, const std::string & name) : codec_type_(type), codec_name_(name) {

    }

    virtual ~Codec() {

    }

    std::string & get_codec_name() {
        return codec_name_;
    }

    CodecType get_codec_type() {
        return codec_type_;
    }

    virtual bool is_ready() {
        return ready_;
    }

    void set_data_rate(uint32_t v) {
        data_rate_ = v;
    }

    uint32_t get_data_rate() {
        return data_rate_;
    }

    virtual Json::Value to_json() {
        Json::Value v;
        return v;
    }
protected:
    CodecType   codec_type_;
    std::string codec_name_;
protected:
    bool ready_ = false;
    uint32_t data_rate_;
};

};