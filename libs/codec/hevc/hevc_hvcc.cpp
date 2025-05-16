#include "spdlog/spdlog.h"

#include <arpa/inet.h>
#include <iostream>
#include "hevc_hvcc.hpp"
#include "hevc_define.hpp"

using namespace mms;
int32_t HEVCDecoderConfigurationRecord::size() {
     int size = 23;
    for (auto & array : arrays) {
        size += 3;
        for (auto & nalu : array.nalus) {
            size += 2;
            size += nalu.nalu.size();
        }
    }
    return size;
}

int32_t HEVCDecoderConfigurationRecord::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return 0;
    }
    version = *data;
    data++;
    len--;

    if (len < 1) {
        return 0;
    }
    
    general_profile_idc = data[0]&0x1f;
    general_tier_flag = (data[0]>>5)&0x01;
    general_profile_space = (data[0]>>6)&0x03;
    data++;
    len--;
    if (len < 4) {
        return 0;
    }
    general_profile_compatibility_flags = ntohl(*(uint32_t*)data);
    data += 4;
    len -= 4;

    if (len < 6) {
        return 0;
    }
    memcpy(general_constraint_indicator_flags, data, 6);
    data += 6;
    len -= 6;
    
    if (len < 1) {
        return 0;
    }
    general_level_idc = data[0];
    data++;
    len--;

    if (len < 2) {
        return 0;
    }
    uint16_t tmp = ntohs(*(uint16_t*)data);
    min_spatial_segmentation_idc = tmp&0xF;
    reserved1 = (tmp>>4)&0xFFF;
    data += 2;
    len -= 2;
    
    if (len < 1) {
        return 0;
    }
    parallelismType = data[0]&0x3;
    reserved2 = (data[0]>>2)&0x3F;
    data++;
    len--;

    if (len < 1) {
        return 0;
    }
    chromaFormat = data[0]&0x03;
    reserved3 = (data[0]>>2)&0x3f;
    data++;
    len--;

    if (len < 1) {
        return 0;
    }
    bitDepthLumaMinus8 = data[0]&0x07;
    reserved4 = (data[0]>>3)&0x1F;
    data++;
    len--;

    if (len < 1){
        return 0;
    }
    bitDepthChromaMinus8 = data[0]&0x07;
    reserved5 = (data[0]>>3)&0x1F;
    data++;
    len--;

    if (len < 2) {
        return 0;
    }
    avgFrameRate = ntohs(*(uint16_t*)data);
    data += 2;
    len -= 2;

    if (len < 1) {
        return 0;
    }

    lengthSizeMinusOne = data[0]&0x03;
    temporalIdNested = (data[0]>>2)&0x01;
    numTemporalLayers = (data[0]>>3)&0x07;
    constantFrameRate = (data[0]>>6)&0x03;
    data++;
    len--;

    if (len < 1) {
        return 0;
    }
    numOfArrays = data[0];
    data++;
    len--;
    for (int i = 0; i < numOfArrays; i++) {
        HVCCNALUnitArray nal_unit_array;
        int32_t consumed = nal_unit_array.decode(data, len);
        if (consumed == 0) {
            return 0;
        }
        data += consumed;
        len -= consumed;
        arrays.push_back(nal_unit_array);

        for (int j = 0; j < (int)nal_unit_array.nalus.size(); j++) {
            HEVC_NALU_TYPE nal_type = H265_TYPE(nal_unit_array.nalus[j].nalu[0]);
            if (nal_type == NAL_VPS) {
                vps_nalu_ = nal_unit_array.nalus[j].nalu;
            } else if (nal_type == NAL_SPS) {
                sps_nalu_ = nal_unit_array.nalus[j].nalu;
            } else if (nal_type == NAL_PPS) {
                pps_nalu_ = nal_unit_array.nalus[j].nalu;
            }
        }
    }
    return data - data_start;
}

int32_t HEVCDecoderConfigurationRecord::encode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    *data = version;
    data++;
    len--;

    if (len < 1) {
        return -2;
    }
    *data = (general_profile_space << 6) | (general_tier_flag << 5) | general_profile_idc;
    data++;
    len--;

    if (len < 4) {
        return -3;
    }
    *(uint32_t*)data = htonl(general_profile_compatibility_flags);
    data += 4;
    len -= 4;

    if (len < 6) {
        return -4;
    }
    memcpy(data, general_constraint_indicator_flags, 6);
    data += 6;
    len -= 6;

    if (len < 1) {
        return -5;
    }
    *data = general_level_idc;
    data++;
    len--;

    if (len < 2) {
        return -6;
    }
    *(uint16_t*)data = htons((reserved1 << 12) | min_spatial_segmentation_idc);
    data += 2;
    len -= 2;

    if (len < 1) {
        return -7;
    }
    *data = (reserved2 << 6) | parallelismType;
    data++;
    len--;

    if (len < 1) {
        return -7;
    }
    *data = (reserved3 << 2) | chromaFormat;
    data++;
    len--;

    if (len < 1) {
        return -7;
    }
    *data = (reserved4 << 3) | bitDepthLumaMinus8;
    data++;
    len--;

    if (len < 1) {
        return -7;
    }
    *data = (reserved5 << 3) | bitDepthChromaMinus8;
    data++;
    len--;

    if (len < 2) {
        return -7;
    }
    *(uint16_t*)data = htons(avgFrameRate);
    data += 2;
    len -= 2;

    if (len < 1) {
        return -7;
    }
    *data = (constantFrameRate << 6) | (numTemporalLayers << 4) | (temporalIdNested << 2) | (lengthSizeMinusOne);
    data++;
    len--;

    if (len < 1) {
        return -7;
    }
    *data = numOfArrays;
    data++;
    len--;
    for (int i = 0; i < numOfArrays; i++) {
        int32_t consumed = arrays[i].encode(data, len);
        if (consumed < 0) {
            return -8;
        }
        data += consumed;
        len -= consumed;
    }
    return data - data_start;
}

int32_t HVCCNALUnitArray::encode(uint8_t *data, size_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    *data = (array_completeness << 7) | (reserved << 6) | (NAL_unit_type);
    data++;
    len--;
    
    if (len < 2) {
        return -2;
    }
    *(uint16_t*)data = htons(numNalus);
    data += 2;
    len -= 2;
    
    for (int i = 0; i < numNalus; i++) {
        if (len < 2) {
            return -3;
        }
        *(uint16_t*)data = htons(nalus[i].nalUnitLength);
        data += 2;
        len -= 2;

        if (len < nalus[i].nalUnitLength) {
            return -4;
        }
        memcpy(data, nalus[i].nalu.data(), nalus[i].nalUnitLength);
        data += nalus[i].nalUnitLength;
        len -= nalus[i].nalUnitLength;
    }
    return data - data_start;
}

int32_t HVCCNALUnitArray::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return 0;
    }

    NAL_unit_type = data[0]&0x3F;
    reserved = (data[0]>>6)&0x01;
    array_completeness = (data[0]>>7)&0x01;
    data++;
    len--;
    
    if (len < 2) {
        return 0;
    }
    numNalus = ntohs(*(uint16_t*)data);
    data += 2;
    len -= 2;

    for (int i = 0; i < numNalus; i++) {
        if (len < 2) {
            return 0;
        }
        Nalu nalu;
        nalu.nalUnitLength = ntohs(*(uint16_t*)data);
        data += 2;
        len -= 2;

        if (len < nalu.nalUnitLength) {
            return 0;
        }
        nalu.nalu.assign((char*)data, nalu.nalUnitLength);
        data += nalu.nalUnitLength;
        len -= nalu.nalUnitLength;
        nalus.push_back(nalu);
    }
    return data - data_start;
}

void HEVCDecoderConfigurationRecord::add_nalu(uint8_t type, const std::string & nalu) {
    HVCCNALUnitArray arr;
    HVCCNALUnitArray::Nalu n;
    n.nalUnitLength = nalu.size();
    n.nalu = nalu;
    arr.NAL_unit_type = type;
    arr.numNalus++;
    arr.nalus.push_back(n);
    arrays.push_back(arr);
    numOfArrays++;
}

const std::string & HEVCDecoderConfigurationRecord::get_sps() {
    return sps_nalu_;
}

const std::string & HEVCDecoderConfigurationRecord::get_pps() {
    return pps_nalu_;
}

const std::string & HEVCDecoderConfigurationRecord::get_vps() {
    return vps_nalu_;
}