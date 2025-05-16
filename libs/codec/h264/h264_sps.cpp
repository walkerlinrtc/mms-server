#include "h264_sps.hpp"
#include "base/utils/bit_stream.hpp"
#include "spdlog/spdlog.h"
using namespace mms;
#define Extended_SAR 0xff
int vui_parameters_t::parse(BitStream & bit_stream) {
    if (!bit_stream.read_one_bit(aspect_ratio_info_present_flag)) {
        return -1;
    }

    if (aspect_ratio_info_present_flag) {
        if (!bit_stream.read_u(8, aspect_ratio_idc)) {
            return -2;
        }

        if (aspect_ratio_idc == Extended_SAR) {
            if (!bit_stream.read_u(16, sar_width)) {
                return -3;
            }

            if (!bit_stream.read_u(16, sar_height)) {
                return -4;
            }
        }
    }

    if (!bit_stream.read_one_bit(overscan_info_present_flag)) {
        return -5;
    }

    if (overscan_info_present_flag) {
        if (!bit_stream.read_one_bit(overscan_appropriate_flag)) {
            return -6;
        }
    }

    if (!bit_stream.read_one_bit(video_signal_type_present_flag)) {
        return -7;
    }

    if (video_signal_type_present_flag) {
        if (!bit_stream.read_u(3, video_format)) {
            return -8;
        }

        if (!bit_stream.read_one_bit(video_full_range_flag)) {
            return -9;
        }

        if (!bit_stream.read_one_bit(colour_description_present_flag)) {
            return -10;
        }

        if (colour_description_present_flag) {
            if (!bit_stream.read_u(8, colour_primaries)) {
                return -11;
            }

            if (!bit_stream.read_u(8, transfer_characteristics)) {
                return -12;
            }

            if (!bit_stream.read_u(8, matrix_coefficients)) {
                return -13;
            }
        }
    }

    if (!bit_stream.read_one_bit(chroma_loc_info_present_flag)) {
        return -14;
    }

    if (chroma_loc_info_present_flag) {
        if (!bit_stream.read_exp_golomb_ue(chroma_sample_loc_type_top_field)) {
            return -15;
        }

        if (!bit_stream.read_exp_golomb_ue(chroma_sample_loc_type_bottom_field)) {
            return -16;
        }
    }

    if (!bit_stream.read_one_bit(timing_info_present_flag)) {
        return -17;
    }

    if (timing_info_present_flag) {
        if (!bit_stream.read_u(32, num_units_in_tick)) {
            return -18;
        }

        if (!bit_stream.read_u(32, time_scale)) {
            return -19;
        }

        if (!bit_stream.read_one_bit(fixed_frame_rate_flag)) {
            return -20;
        }
    }

    if (!bit_stream.read_one_bit(nal_hrd_parameters_present_flag)) {
        return -21;
    }

    if (nal_hrd_parameters_present_flag) {
        if (0 != hrd_parameters1.parse(bit_stream)) {
            return -22;
        }
    }

    if (!bit_stream.read_one_bit(vcl_hrd_parameters_present_flag)) {
        return -23;
    }

    if (vcl_hrd_parameters_present_flag) {
        int ret = hrd_parameters2.parse(bit_stream);
        if (0 != ret) {
            return -24;
        }
    }

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        if (!bit_stream.read_one_bit(low_delay_hrd_flag)) {
            return -25;
        }
    }

    if (!bit_stream.read_one_bit(pic_struct_present_flag)) {
        return -26;
    }

    if (!bit_stream.read_one_bit(bitstream_restriction_flag)) {
        return -27;
    }

    if (bitstream_restriction_flag) {
        if (!bit_stream.read_one_bit(motion_vectors_over_pic_boundaries_flag)) {
            return -28;
        }

        if (!bit_stream.read_exp_golomb_ue(max_bytes_per_pic_denom)) {
            return -29;
        }

        if (!bit_stream.read_exp_golomb_ue(max_bits_per_mb_denom)) {
            return -30;
        }

        if (!bit_stream.read_exp_golomb_ue(log2_max_mv_length_horizontal)) {
            return -31;
        }

        if (!bit_stream.read_exp_golomb_ue(log2_max_mv_length_vertical)) {
            return -32;
        }

        if (!bit_stream.read_exp_golomb_ue(max_num_reorder_frames)) {
            return -33;
        }

        if (!bit_stream.read_exp_golomb_ue(max_dec_frame_buffering)) {
            return -34;
        }
    }

    return 0;
}

int hrd_parameters_t::parse(BitStream & bit_stream) {
    if (!bit_stream.read_exp_golomb_ue(cpb_cnt_minus1)) {
        return -1;
    }

    if (!bit_stream.read_u(4, bit_rate_scale)) {
        return -2;
    }

    if (!bit_stream.read_u(4, cpb_size_scale)) {
        return -3;
    }

    for (uint64_t SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++) {
        uint64_t v;
        if (!bit_stream.read_exp_golomb_ue(v)) {
            return -4;
        }
        bit_rate_value_minus1.push_back(v);

        if (!bit_stream.read_exp_golomb_ue(v)) {
            return -5;
        }
        cpb_size_value_minus1.push_back(v);

        uint8_t b;
        if (!bit_stream.read_one_bit(b)) {
            return -6;
        }
        cbr_flag.push_back(b);
    }

    if (!bit_stream.read_u(5, initial_cpb_removal_delay_length_minus1)) {
        return -7;
    }

    if (!bit_stream.read_u(5, cpb_removal_delay_length_minus1)) {
        return -8;
    }

    if (!bit_stream.read_u(5, dpb_output_delay_length_minus1)) {
        return -9;
    }

    if (!bit_stream.read_u(5, time_offset_length)) {
        return -10;
    }
    return 0;
}

int h264_sps_t::parse(BitStream & bit_stream) { 
    if (!bit_stream.read_u(8, profile_idc)) {
        return -1;
    }

    if (!bit_stream.read_one_bit(constraint_set0_flag)) {
        return -2;
    }

    if (!bit_stream.read_one_bit(constraint_set1_flag)) {
        return -3;
    }

    if (!bit_stream.read_one_bit(constraint_set2_flag)) {
        return -4;
    }

    if (!bit_stream.read_one_bit(constraint_set3_flag)) {
        return -5;
    }

    if (!bit_stream.read_one_bit(constraint_set4_flag)) {
        return -6;
    }

    if (!bit_stream.read_one_bit(constraint_set5_flag)) {
        return -7;
    }

    if (!bit_stream.read_u(2, reserved_zero_2bits)) {
        return -8;
    }

    if (!bit_stream.read_u(8, level_idc)) {
        return -9;
    }

    if (!bit_stream.read_exp_golomb_ue(seq_parameter_set_id)) {
        return -10;
    }

    if( profile_idc == 100 || profile_idc == 110 ||
        profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
        profile_idc == 83 || profile_idc == 86 || profile_idc == 118 ||
        profile_idc == 128 || profile_idc == 138 || profile_idc == 139 ||
        profile_idc == 134 || profile_idc == 135 ) {
        if (!bit_stream.read_exp_golomb_ue(chroma_format_idc)) {
            return -11;
        }

        if (chroma_format_idc == 3) {
            if (!bit_stream.read_one_bit(separate_colour_plane_flag)) {
                return -12;
            }
        }

        if (!bit_stream.read_exp_golomb_ue(bit_depth_luma_minus8)) {
            return -13;
        }

        if (!bit_stream.read_exp_golomb_ue(bit_depth_chroma_minus8)) {
            return -14;
        }

        if (!bit_stream.read_one_bit(qpprime_y_zero_transform_bypass_flag)) {
            return -15;
        }

        if (!bit_stream.read_one_bit(seq_scaling_matrix_present_flag)) {
            return -16;
        }

        if (seq_scaling_matrix_present_flag) {
            uint64_t len;
            if (chroma_format_idc != 3) {
                len = 8;
            } else {
                len = 12;
            }

            for (uint64_t i = 0; i < len; i++) {
                uint8_t b = 0;
                if (!bit_stream.read_one_bit(b)) {
                    return false;
                }
                seq_scaling_list_present_flag.push_back(b);
                // todo: scaling list
                // if (seq_scaling_list_present_flag[i]) {
                //     if (i < 6) {
                //     }
                // }
            }
        }
    }

    if (!bit_stream.read_exp_golomb_ue(log2_max_frame_num_minus4)) {
        return -17;
    }

    if (!bit_stream.read_exp_golomb_ue(pic_order_cnt_type)) {
        return -18;
    }

    if (pic_order_cnt_type == 0) {
        if (!bit_stream.read_exp_golomb_ue(log2_max_pic_order_cnt_lsb_minus4)) {
            return -19;
        } 
    } else if (pic_order_cnt_type == 1) {
        if (!bit_stream.read_one_bit(delta_pic_order_always_zero_flag)) {
            return -20;
        }

        if (!bit_stream.read_exp_golomb_se(offset_for_non_ref_pic)) {
            return -21;
        }

        if (!bit_stream.read_exp_golomb_se(offset_for_top_to_bottom_field)) {
            return -22;
        }

        if (!bit_stream.read_exp_golomb_ue(num_ref_frames_in_pic_order_cnt_cycle)) {
            return -23;
        }

        for (uint64_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
            int64_t v;
            if (!bit_stream.read_exp_golomb_se(v)) {
                return -24;
            }
            offset_for_ref_frame.push_back(v);
        }
    }

    if (!bit_stream.read_exp_golomb_ue(max_num_ref_frames)) {
        return -25;
    }

    if (!bit_stream.read_one_bit(gaps_in_frame_num_value_allowed_flag)) {
        return -26;
    }

    if (!bit_stream.read_exp_golomb_ue(pic_width_in_mbs_minus1)) {
        return -27;
    }

    if (!bit_stream.read_exp_golomb_ue(pic_height_in_map_units_minus1)) {
        return -28;
    }

    if (!bit_stream.read_one_bit(frame_mbs_only_flag)) {
        return -29;
    }

    if (!frame_mbs_only_flag) {
        if (!bit_stream.read_one_bit(mb_adaptive_frame_field_flag)) {
            return -30;
        }
    }

    if (!bit_stream.read_one_bit(direct_8x8_inference_flag)) {
        return -31;
    }

    if (!bit_stream.read_one_bit(frame_cropping_flag)) {
        return -32;
    }

    if (frame_cropping_flag) {
        if (!bit_stream.read_exp_golomb_ue(frame_crop_left_offset)) {
            return -33;
        }

        if (!bit_stream.read_exp_golomb_ue(frame_crop_right_offset)) {
            return -34;
        }

        if (!bit_stream.read_exp_golomb_ue(frame_crop_top_offset)) {
            return -35;
        }

        if (!bit_stream.read_exp_golomb_ue(frame_crop_bottom_offset)) {
            return -36;
        }
    }

    if (!bit_stream.read_one_bit(vui_parameters_present_flag)) {
        return -37;
    }

    if (vui_parameters_present_flag == 1) {
        int ret = vui_parameters.parse(bit_stream);
        if (0 != ret) {
            return -38;
        }
    }

    return 0;
}