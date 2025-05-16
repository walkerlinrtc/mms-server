//7.3.2.1.1 Sequence parameter set data syntax
// seq_parameter_set_data( ) { C Descriptor
//     profile_idc 0 u(8)
//     constraint_set0_flag 0 u(1)
//     constraint_set1_flag 0 u(1)
//     constraint_set2_flag 0 u(1)
//     constraint_set3_flag 0 u(1)
//     constraint_set4_flag 0 u(1)
//     constraint_set5_flag 0 u(1)
//     reserved_zero_2bits /* equal to 0 */ 0 u(2)
//     level_idc 0 u(8)
//     seq_parameter_set_id 0 ue(v)
//     if( profile_idc = = 100 | | profile_idc = = 110 | |
//     profile_idc = = 122 | | profile_idc = = 244 | | profile_idc = = 44 | |
//     profile_idc = = 83 | | profile_idc = = 86 | | profile_idc = = 118 | |
//     profile_idc = = 128 | | profile_idc = = 138 | | profile_idc = = 139 | |
//     profile_idc = = 134 | | profile_idc = = 135 ) {
//          chroma_format_idc 0 ue(v)
//          if( chroma_format_idc = = 3 )
//              separate_colour_plane_flag 0 u(1)
//          bit_depth_luma_minus8 0 ue(v)
//          bit_depth_chroma_minus8 0 ue(v)
//          qpprime_y_zero_transform_bypass_flag 0 u(1)
//          seq_scaling_matrix_present_flag 0 u(1)
//          if( seq_scaling_matrix_present_flag )
//              for( i = 0; i < ( ( chroma_format_idc != 3 ) ? 8 : 12 ); i++ ) {
//                  seq_scaling_list_present_flag[ i ] 0 u(1)
//                  if( seq_scaling_list_present_flag[ i ] )
//                      if( i < 6 ) 
//                          scaling_list( ScalingList4x4[ i ], 16, UseDefaultScalingMatrix4x4Flag[ i ] ) 0
//                      else
//                          scaling_list( ScalingList8x8[ i − 6 ], 64, UseDefaultScalingMatrix8x8Flag[ i − 6 ] )
//              } 
//          }
//          log2_max_frame_num_minus4 0 ue(v)
//          pic_order_cnt_type 0 ue(v)
//          if( pic_order_cnt_type = = 0 )
//              log2_max_pic_order_cnt_lsb_minus4 0 ue(v)
//          else if( pic_order_cnt_type = = 1 ) {
//              delta_pic_order_always_zero_flag 0 u(1)
//              offset_for_non_ref_pic 0 se(v)
//              offset_for_top_to_bottom_field 0 se(v)
//              num_ref_frames_in_pic_order_cnt_cycle 0 ue(v)
//              for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
//                  offset_for_ref_frame[ i ] 0 se(v)
//          }
//          max_num_ref_frames 0 ue(v)
//          gaps_in_frame_num_value_allowed_flag 0 u(1)
//          pic_width_in_mbs_minus1 0 ue(v)
//          pic_height_in_map_units_minus1 0 ue(v)
//          frame_mbs_only_flag 0 u(1)
//          if( !frame_mbs_only_flag )
//              mb_adaptive_frame_field_flag 0 u(1)
//          direct_8x8_inference_flag 0 u(1)
//          frame_cropping_flag 0 u(1)
//          if( frame_cropping_flag ) {
//              frame_crop_left_offset 0 ue(v)
//              frame_crop_right_offset 0 ue(v)
//              frame_crop_top_offset 0 ue(v)
//              frame_crop_bottom_offset 0 ue(v)
//          }
//          vui_parameters_present_flag 0 u(1)
//          if( vui_parameters_present_flag )
//              vui_parameters( ) 
//}
#pragma once 
#include <vector>
#include <stdint.h>
namespace mms {
class BitStream;

class hrd_parameters_t {
public:
    uint64_t cpb_cnt_minus1;//ue(v)
    uint8_t bit_rate_scale;//u(4)
    uint8_t cpb_size_scale;//u(4)
    std::vector<uint64_t> bit_rate_value_minus1;//ue(v)
    std::vector<uint64_t> cpb_size_value_minus1;//ue(v)
    std::vector<uint8_t> cbr_flag;//u(1)
    uint8_t initial_cpb_removal_delay_length_minus1;//u(5)
    uint8_t cpb_removal_delay_length_minus1;//u(5)
    uint8_t dpb_output_delay_length_minus1;//u(5)
    uint8_t time_offset_length;//u(5)

    int parse(BitStream & bit_stream);
};

class vui_parameters_t {
public:
    uint8_t aspect_ratio_info_present_flag;//u(1)
    uint8_t aspect_ratio_idc;//u(8)
    uint16_t sar_width;//u(16)
    uint16_t sar_height;//u(16)
    uint8_t overscan_info_present_flag;//u(1)
    uint8_t overscan_appropriate_flag;//u(1)
    uint8_t video_signal_type_present_flag;//u(1)
    uint8_t video_format;//u(3)
    uint8_t video_full_range_flag;//u(1)
    uint8_t colour_description_present_flag;//u(1)
    uint8_t colour_primaries;//u(8)
    uint8_t transfer_characteristics;//u(8)
    uint8_t matrix_coefficients;//u(8)
    uint8_t chroma_loc_info_present_flag;//u(1)
    uint64_t chroma_sample_loc_type_top_field;//ue(v)
    uint64_t chroma_sample_loc_type_bottom_field;//ue(v)
    uint8_t timing_info_present_flag;//u(1)
    uint64_t num_units_in_tick = 0;//u(32)
    uint64_t time_scale = 0;//u(32) // fps=time_scale/num_units_in_tick;
    uint8_t fixed_frame_rate_flag;//u(1)
    uint8_t nal_hrd_parameters_present_flag;//u(1)
    hrd_parameters_t hrd_parameters1;
    uint8_t vcl_hrd_parameters_present_flag;//u(1)
    hrd_parameters_t hrd_parameters2;
    uint8_t low_delay_hrd_flag;//u(1)
    uint8_t pic_struct_present_flag;//u(1)
    uint8_t bitstream_restriction_flag;//u(1)
    uint8_t motion_vectors_over_pic_boundaries_flag;//u(1)
    uint64_t max_bytes_per_pic_denom;//ue(v)
    uint64_t max_bits_per_mb_denom;//ue(v)
    uint64_t log2_max_mv_length_horizontal;//ue(v)
    uint64_t log2_max_mv_length_vertical;//ue(v)
    uint64_t max_num_reorder_frames;//ue(v)
    uint64_t max_dec_frame_buffering;//ue(v)

    int parse(BitStream & bit_stream);
};


class h264_sps_t {
public:
    uint8_t profile_idc;            //u(8)
    uint8_t constraint_set0_flag;   //u(1)
    uint8_t constraint_set1_flag;   //u(1)
    uint8_t constraint_set2_flag;   //u(1)
    uint8_t constraint_set3_flag;   //u(1)
    uint8_t constraint_set4_flag;   //u(1)
    uint8_t constraint_set5_flag;   //u(1)
    uint8_t reserved_zero_2bits;    //u(2)
    uint8_t level_idc;              //u(8)
    uint64_t seq_parameter_set_id;  //ue(v)
    uint64_t chroma_format_idc;     //ue(v)
    uint8_t separate_colour_plane_flag;//u(1)
    uint64_t bit_depth_luma_minus8;//ue(v)
    uint64_t bit_depth_chroma_minus8;//ue(v)
    uint8_t qpprime_y_zero_transform_bypass_flag;//u(1)
    uint8_t seq_scaling_matrix_present_flag;// 0 u(1)
    std::vector<uint8_t> seq_scaling_list_present_flag;
    uint64_t log2_max_frame_num_minus4;// 0 ue(v)
    uint64_t pic_order_cnt_type;// 0 ue(v)
    uint64_t log2_max_pic_order_cnt_lsb_minus4;
    uint8_t delta_pic_order_always_zero_flag;// 0 u(1)
    int64_t offset_for_non_ref_pic;// 0 se(v)
    int64_t offset_for_top_to_bottom_field;// 0 se(v)
    uint64_t num_ref_frames_in_pic_order_cnt_cycle;// 0 ue(v)
    std::vector<int64_t> offset_for_ref_frame;//
    uint64_t max_num_ref_frames;// 0 ue(v)
    uint8_t gaps_in_frame_num_value_allowed_flag;// 0 u(1)
    uint64_t pic_width_in_mbs_minus1;// 0 ue(v)
    uint64_t pic_height_in_map_units_minus1;// 0 ue(v)
    uint8_t frame_mbs_only_flag;//0 u(1)
    uint8_t mb_adaptive_frame_field_flag;// 0 u(1)
    uint8_t direct_8x8_inference_flag;// 0 u(1)
    uint8_t frame_cropping_flag;// 0 u(1)
    uint64_t frame_crop_left_offset;// 0 ue(v)
    uint64_t frame_crop_right_offset;// 0 ue(v)
    uint64_t frame_crop_top_offset;// 0 ue(v)
    uint64_t frame_crop_bottom_offset;// 0 ue(v)
    uint8_t vui_parameters_present_flag;// 0 u(1)
    vui_parameters_t vui_parameters;
    int parse(BitStream & bit_stream);
};
};


