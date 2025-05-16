/*
 * @Author: jbl19860422
 * @Date: 2023-12-02 21:44:45
 * @LastEditTime: 2023-12-03 17:16:20
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\codec\aac\aac_decoder.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <stdint.h>

#include "aac_decoder.hpp"
#include "faad.h"
#include "adts.hpp"
using namespace mms;
AACDecoder::AACDecoder() {

}

AACDecoder::~AACDecoder() {
    if (handle_) {
        NeAACDecClose((NeAACDecHandle)handle_);
        handle_ = nullptr;
    }
}

bool AACDecoder::init(const std::string & decoder_config) {
    handle_ = (void*)NeAACDecOpen();
    if (!handle_) {
        return false;
    }

    NeAACDecConfigurationPtr conf = NeAACDecGetCurrentConfiguration((NeAACDecHandle)handle_);
	conf->defObjectType = LC; //aac "MAIN"
	conf->defSampleRate = 44100;
	conf->outputFormat = FAAD_FMT_16BIT;
    conf->useOldADTSFormat = 0;
	conf->downMatrix = 1;
	int ret = NeAACDecSetConfiguration((NeAACDecHandle)handle_, conf); //set the stuff
    if (ret != 1) {
        NeAACDecClose((NeAACDecHandle)handle_);
        handle_ = nullptr;
        return false;
    }

    ret = NeAACDecInit2((NeAACDecHandle)handle_, (unsigned char*)decoder_config.data(), decoder_config.size(), &sample_rate_, &channels_);
    if (ret < 0) {
        NeAACDecClose((NeAACDecHandle)handle_);
        handle_ = nullptr;
        return false;
    }
    return true;
}

unsigned long AACDecoder::get_sample_rate() {
    return sample_rate_;
}

unsigned char AACDecoder::get_channels() {
    return channels_;
}

std::string_view AACDecoder::decode(uint8_t *data_in, int32_t in_len) {
    NeAACDecFrameInfo frame_info;
    auto pcm_data = (unsigned char*)NeAACDecDecode(handle_, 
                                                   &frame_info, 
                                                   data_in, 
                                                   in_len);
    if (pcm_data == NULL) {
        return {};
    }

    if (frame_info.error != 0) {
        return {};
    }

    if (frame_info.samples <= 0) {
        return {};
    }
    return std::string_view((char*)pcm_data, frame_info.samples*2);//frame_info.samples包含了所有通道的样本总和，16位深度
}