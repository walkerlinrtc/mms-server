#include "spdlog/spdlog.h"

#include <arpa/inet.h>
#include <iostream>
#include "h264_avcc.hpp"
using namespace mms;

int32_t AVCDecoderConfigurationRecord::parse(uint8_t *data, size_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    version = data[0];
    data++;
    len--;

    if (len < 1) {
        return -2;
    }
    avc_profile = data[0];
    data++;
    len--;

    if (len < 1) {
        return -3;
    }
    avc_compatibility = data[0];
    data++;
    len--;

    if (len < 1) {
        return -4;
    }
    avc_level = data[0];
    data++;
    len--;

    if (len < 1) {
        return -5;
    }
    nalu_length_size_minus_one = data[0] & 0x03;
    data++;
    len--;

    if (len < 1) {
        return -6;
    }
    num_of_sequence_parameter_sets = data[0] & 0x1f;
    data++;
    len--;
    for (auto i = 0; i < num_of_sequence_parameter_sets; i++) {
        uint16_t sps_len = 0;
        if (len < 2) {
            return -7;
        }
        sps_len = ntohs(*(uint16_t*)data);
        data += 2;
        len -= 2;

        if (len < sps_len) {
            return -8;
        }
        sps_nalus.emplace_back(std::string((char*)data, sps_len));
        data += sps_len;
        len -= sps_len;
    }

    if (len < 1) {
        return -9;
    }
    num_of_picture_parameter_sets = data[0];
    data++;
    len--;

    for (auto i = 0; i < num_of_picture_parameter_sets; i++) {
        if (len < 2) {
            return -10;
        }
        uint16_t pps_len = 0;
        pps_len = ntohs(*(uint16_t*)data);
        data += 2;
        len -= 2;

        if (len < pps_len) {
            return -11;
        }
        pps_nalus.emplace_back(std::string((char*)data, pps_len));
        data += pps_len;
        len -= pps_len;
    }
    return data - data_start;
}

int32_t AVCDecoderConfigurationRecord::size() {
    int32_t s = 7;
    for (auto & sps : sps_nalus) {
        s += 2 + sps.size();
    }

    for (auto & pps : pps_nalus) {
        s += 2 + pps.size();
    }
    return s;
}

void AVCDecoderConfigurationRecord::add_sps(const std::string & sps) {
    sps_nalus.push_back(sps);
    num_of_sequence_parameter_sets = sps_nalus.size();
}

void AVCDecoderConfigurationRecord::add_pps(const std::string & pps) {
    pps_nalus.push_back(pps);
    num_of_picture_parameter_sets = pps_nalus.size();
}

int32_t AVCDecoderConfigurationRecord::encode(uint8_t *data, size_t len) {
    const uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    *data = version;
    data++;
    len--;

    if (len < 1) {
        return -2;
    }
    *data = avc_profile;
    data++;
    len--;

    if (len < 1) {
        return -3;
    }
    *data = avc_compatibility;
    data++;
    len--;

    if (len < 1) {
        return -4;
    }
    *data = avc_level;
    data++;
    len--;

    if (len < 1) {
        return -5;
    }
    *data = nalu_length_size_minus_one;
    data++;
    len--;

    if (len < 1) {
        return -6;
    }
    *data = num_of_sequence_parameter_sets;
    data++;
    len--;
    for (auto i = 0; i < num_of_sequence_parameter_sets; i++) {
        uint16_t sps_len = 0;
        if (len < 2) {
            return -7;
        }
        sps_len = sps_nalus[i].size();
        *((uint16_t*)data) = htons(sps_len);
        data += 2;
        len -= 2;

        if (len < sps_len) {
            return -8;
        }
        memcpy(data, sps_nalus[i].data(), sps_len);
        data += sps_len;
        len -= sps_len;
    }

    if (len < 1) {
        return -9;
    }
    data[0] = num_of_picture_parameter_sets;
    data++;
    len--;

    for (auto i = 0; i < num_of_picture_parameter_sets; i++) {
        if (len < 2) {
            return -10;
        }
        uint16_t pps_len = pps_nalus[i].size();
        *((uint16_t*)data) = htons(pps_len);
        data += 2;
        len -= 2;

        if (len < pps_len) {
            return -11;
        }
        memcpy(data, pps_nalus[i].data(), pps_len);
        data += pps_len;
        len -= pps_len;
    }
    return data - data_start;
}

const std::string & AVCDecoderConfigurationRecord::get_sps() {
    if (num_of_sequence_parameter_sets <= 0) {
        static std::string emp_str;
        return emp_str;
    }
    return sps_nalus[0];
}

const std::string & AVCDecoderConfigurationRecord::get_pps() {
    if (num_of_picture_parameter_sets <= 0) {
        static std::string emp_str;
        return emp_str;
    }
    return pps_nalus[0];
}