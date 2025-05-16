#pragma once
#include <vector>
#include <string>
#include "h264_sps.hpp"
#include "h264_pps.hpp"

namespace mms {
class AVCDecoderConfigurationRecord {
public:
    uint8_t version = 0x01;
    uint8_t avc_profile;//profile_idc 占用8位 从H.264标准SPS第一个字段profile_idc拷贝而来 指明所用 profile
    uint8_t avc_compatibility = 0x00;//占用8位 从H.264标准SPS拷贝的冗余字节
    uint8_t avc_level;//level_idc 占用8位 从H.264标准SPS第一个字段level_idc拷贝而来 指明所用 level
    uint8_t reserved1;//6bits
    uint8_t nalu_length_size_minus_one;//2bits
    uint8_t reserved2;//3bits
    uint8_t num_of_sequence_parameter_sets;//5bits
    std::vector<std::string> sps_nalus;
    uint8_t num_of_picture_parameter_sets;//8bits
    std::vector<std::string> pps_nalus;
    const std::string & get_sps();
    const std::string & get_pps();
public:
    int32_t parse(uint8_t *data, size_t len);
    int32_t encode(uint8_t *data, size_t len);

    void add_sps(const std::string & sps);
    void add_pps(const std::string & pps);

    int32_t size();
public:
    std::string sps_rbsp_;
    std::string pps_rbsp_;

    h264_sps_t sps_;
};

};