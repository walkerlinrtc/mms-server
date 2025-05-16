#pragma once
#include <memory>
#include "../codec.hpp"
#include "h264_sps.hpp"
#include "h264_pps.hpp"
#include "base/utils/bit_stream.hpp"
#include "h264_avcc.hpp"

namespace mms {
class MediaSdp;
class Payload;
class AVCDecoderConfigurationRecord;

/**
 * Table 7-1 - NAL unit type codes, syntax element categories, and NAL unit type classes
 * ISO_IEC_14496-10-AVC-2012.pdf, page 83.
 */
enum H264NaluType
{
    // Unspecified
    H264NaluTypeReserved = 0,
    H264NaluTypeForbidden = 0,
    // Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
    H264NaluTypeNonIDR = 1,
    // Coded slice data partition A slice_data_partition_a_layer_rbsp( )
    H264NaluTypeDataPartitionA = 2,
    // Coded slice data partition B slice_data_partition_b_layer_rbsp( )
    H264NaluTypeDataPartitionB = 3,
    // Coded slice data partition C slice_data_partition_c_layer_rbsp( )
    H264NaluTypeDataPartitionC = 4,
    // Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
    H264NaluTypeIDR = 5,
    // Supplemental enhancement information (SEI) sei_rbsp( )
    H264NaluTypeSEI = 6,
    // Sequence parameter set seq_parameter_set_rbsp( )
    H264NaluTypeSPS = 7,
    // Picture parameter set pic_parameter_set_rbsp( )
    H264NaluTypePPS = 8,
    // Access unit delimiter access_unit_delimiter_rbsp( )
    H264NaluTypeAccessUnitDelimiter = 9,
    // End of sequence end_of_seq_rbsp( )
    H264NaluTypeEOSequence = 10,
    // End of stream end_of_stream_rbsp( )
    H264NaluTypeEOStream = 11,
    // Filler data filler_data_rbsp( )
    H264NaluTypeFilterData = 12,
    // Sequence parameter set extension seq_parameter_set_extension_rbsp( )
    H264NaluTypeSPSExt = 13,
    // Prefix NAL unit prefix_nal_unit_rbsp( )
    H264NaluTypePrefixNALU = 14,
    // Subset sequence parameter set subset_seq_parameter_set_rbsp( )
    H264NaluTypeSubsetSPS = 15,
    // Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
    H264NaluTypeLayerWithoutPartition = 19,
    // Coded slice extension slice_layer_extension_rbsp( )
    H264NaluTypeCodedSliceExt = 20,
};

class H264Codec : public Codec {
public:
    H264Codec() : Codec(CODEC_H264, "H264") {

    }

    bool set_sps_pps(const std::string & sps, const std::string & pps);
    static std::shared_ptr<H264Codec> create_from_sdp(const MediaSdp & media_sdp, const Payload & payload);
    static std::shared_ptr<H264Codec> create_from_avcc(AVCDecoderConfigurationRecord & avc_configuration);

    const std::string & get_sps_nalu() const {
        return sps_nalu_;
    }

    const std::string & get_pps_nalu() const {
        return pps_nalu_;
    }

    const std::string & get_sps_rbsp() const {
        return sps_rbsp_;
    }

    const std::string & get_pps_rbsp() const {
        return pps_rbsp_;
    }

    void set_sps(const std::string & sps);
    void set_pps(const std::string & pps);
    bool get_wh(uint32_t & w, uint32_t & h);

    bool get_fps(double & fps);

    void set_avc_configuration(AVCDecoderConfigurationRecord & avc_configuration) {
        avc_configuration_ = avc_configuration;
        avc_configuration_valid_ = true;
    }

    AVCDecoderConfigurationRecord & get_avc_configuration();
    std::shared_ptr<Payload> get_payload();
private:
    bool gen_avc_decoder_configuration_record();
    void deemulation_prevention(const std::string_view & input, std::string & output);//去除竞争字段
    bool sps_pps_parsed_ = false;
    std::string sps_nalu_; 
    std::string pps_nalu_;

    std::string sps_rbsp_;
    std::string pps_rbsp_;

    h264_sps_t sps_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    double fps_ = 0;

    bool avc_configuration_valid_ = false;
    std::shared_ptr<Payload> sdp_payload_;
    AVCDecoderConfigurationRecord avc_configuration_;
};
};