#pragma once
#include <stdint.h>
#include <string_view>

#include "ts_pat_pmt.hpp"
#include "base/packet.h"

namespace mms {
enum TrickMode {
    FastForward = 0x00,
    SlowMotion  = 0x01,
    FreezeFrame = 0x02,
    FastReverse = 0x03,
    SlowReverse = 0x04,
};

class TsSegment;

struct PESPacketHeader {
    int32_t packet_start_code_prefix;//24bit
    TsPESStreamId stream_id;//8bit
    uint16_t PES_packet_length;//16bit
    
    int8_t const_value1:2;//'10'
    int8_t PES_scrambling_control:2;//2bit
    int8_t PES_priority:1;
    int8_t data_alignment_indicator:1;
    int8_t copyright:1;
    int8_t original_or_copy:1;
    int8_t PTS_DTS_flags = 0;
    int8_t ESCR_flag:1;
    int8_t ES_rate_flag:1;
    int8_t DSM_trick_mode_flag:1;
    int8_t additional_copy_info_flag:1;
    int8_t PES_CRC_flag:1;
    int8_t PES_extension_flag:1;
    uint8_t PES_header_data_length;
    int64_t pts = 0;//33bit
    int64_t dts = 0;
    int64_t ESCR_base:33;//
    int32_t ES_rate;//22bit
    TrickMode trick_mode_control:3;
    int8_t field_id:2;
    int8_t intra_slice_refresh:1;
    int8_t frequency_truncation:2;
    int8_t rep_cntrl:5;
    int8_t additional_copy_info:7;
    int16_t previous_PES_packet_CRC:16;
    int8_t PES_private_data_flag:1;
    int8_t pack_header_field_flag:1;
    int8_t program_packet_sequence_counter_flag:1;
    int8_t P_STD_buffer_flag:3;
    int8_t PES_extension_flag_2:1;
    int8_t PES_private_data[128];
    int8_t pack_field_length;
    int8_t program_packet_sequence_counter:7;
    int8_t MPEG1_MPEG2_identifier:1;
    int8_t original_stuff_length:6;
    int8_t P_STD_buffer_scale:1;
    int16_t P_STD_buffer_size:13;
    int8_t PES_extension_field_length:7;
    std::string_view PES_packet_data;

    int32_t decode(const char *data, int32_t len);
};

struct PESPacket : public Packet {
    bool is_key = false;
    PESPacketHeader pes_header;
    // std::string_view pes_header_sv;
    std::string_view pes_payload;

    std::shared_ptr<TsSegment> ts_seg;//ts切片holder
    int32_t ts_index = 0;
    int32_t ts_off = 0;//ts在ts_seg中的偏移
    int32_t ts_bytes = 0;//ts在ts_seg中的字节数
    int32_t decode(const char *data, int32_t len);
};

};