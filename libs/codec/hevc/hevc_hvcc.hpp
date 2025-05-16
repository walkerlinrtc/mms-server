#pragma once
#include <stdint.h>
#include <vector>
#include <string>

namespace mms {
struct HVCCNALUnitArray {
    uint8_t array_completeness;//1bit
    uint8_t reserved = 0;//1bit
    uint8_t NAL_unit_type;//6bit
    uint16_t numNalus = 0;//16bit
    struct Nalu {
        uint16_t nalUnitLength = 0;
        std::string nalu;
    };

    int32_t encode(uint8_t *data, size_t len);
    int32_t decode(uint8_t *data, int32_t len);
    std::vector<Nalu> nalus;
};

class HEVCDecoderConfigurationRecord {
public:
    uint8_t  version = 0x01;            //8bit
    uint8_t  general_profile_space = 0;//2bit
    uint8_t  general_tier_flag = 0;//1bit
    uint8_t  general_profile_idc = 0;//5bit
    uint32_t general_profile_compatibility_flags = 0;//32bit
    uint8_t  general_constraint_indicator_flags[6] = {0};//48bit
    uint8_t  general_level_idc = 0;//8bit
    uint8_t  reserved1 = 0xf;// bit(4) reserved = ‘1111’b;
    uint8_t  min_spatial_segmentation_idc = 0;//12bit
    uint8_t  reserved2 = 0x3f;//bit(6) reserved = ‘111111’b;
    uint8_t  parallelismType;//2bit
    uint8_t  reserved3 = 0x3f;//bit(6) reserved = ‘111111’b;
    uint8_t  chromaFormat;//2bit
    uint8_t  reserved4 = 0x1f;//bit(5) reserved = ‘11111’b;
    uint8_t  bitDepthLumaMinus8;//3bit
    uint8_t  reserved5 = 0x1f;//bit(5) reserved = ‘11111’b;
    uint8_t  bitDepthChromaMinus8;//3bit
    uint16_t avgFrameRate = 30;//16bit
    uint8_t  constantFrameRate;//2bit
    uint8_t  numTemporalLayers = 0;//3bit
    uint8_t  temporalIdNested = 0;//1bit
    uint8_t  lengthSizeMinusOne = 3;//2bit
    uint8_t  numOfArrays = 0;
    std::vector<HVCCNALUnitArray> arrays;
    // self define
    std::string vps_nalu_;
    std::string sps_nalu_;
    std::string pps_nalu_;

    const std::string & get_sps();
    const std::string & get_pps();
    const std::string & get_vps();
public:
    int32_t encode(uint8_t *data, int32_t len);
    int32_t decode(uint8_t *data, int32_t len);
    int32_t size();
    void add_nalu(uint8_t type, const std::string & nalu);
public:
};

};