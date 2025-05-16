#include "ts_pes.hpp"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

using namespace mms;

#define AV_RB16(x) \
    ((((const uint8_t*)(x))[0] << 8) |          \
           ((const uint8_t*)(x))[1])
       
static inline int64_t ff_parse_pes_pts(const char *buf) 
{
    return (int64_t)(*buf & 0x0e) << 29 |
            (AV_RB16(buf+1) >> 1) << 15 |
             AV_RB16(buf+3) >> 1;
}

int32_t PESPacketHeader::decode(const char *data, int32_t len) {
    const char *data_start = data;
    static char code_prefix[3] = {0x00, 0x00, 0x01};
    if (len < 3) {
        return 0;
    }
    
    if (memcmp(data, code_prefix, 3) != 0) {
        // CORE_ERROR("pes packet header is not 00 00 01");
        return -1;
    }
    data += 3;
    len -= 3;

    if (len < 1) {
        return 0;
    }
    stream_id = (TsPESStreamId)(*data);
    data++;
    len--;

    if (len < 2) {
        return 0;
    }
    PES_packet_length = ntohs(*(uint16_t*)data);
    // if (PES_packet_length == 0) {
    //     PES_packet_length = len - 6;
    // }
    data += 2;
    len -= 2;
    if (len < PES_packet_length) {
        return 0;
    }
    
    // const char *pes_data_start = data;
    // const char *pes_data_end = pes_data_start + len - 6;//PES_packet_length可能为0
    // int32_t header_bytes = 0;
    if (stream_id != TsPESStreamIdProgramStreamMap &&
        stream_id != TsPESStreamIdPaddingStream &&
        stream_id != TsPESStreamIdPrivateStream2 &&
        stream_id != TsPESStreamIdEcmStream &&
        stream_id != TsPESStreamIdEmmStream &&
        stream_id != TsPESStreamIdProgramStreamDirectory &&
        stream_id != TsPESStreamIdDsmccStream &&
        stream_id != TsPESStreamIdH2221TypeE) {
        if (len < 1) {
            return 0;
        }
        uint8_t v1 = (uint8_t)*data;
        original_or_copy = v1 & 0x01;
        copyright = (v1 >> 1) & 0x01;
        data_alignment_indicator = (v1 >> 2) & 0x01;
        PES_priority = (v1 >> 3) & 0x01;
        PES_scrambling_control = (v1 >> 4) & 0x03;
        // '10' 不检查
        data++;
        len--;
        
        if (len < 1) {
            return 0;
        }
        uint8_t v2 = (uint8_t)*data;
        PES_extension_flag = (v2 >> 0) & 0x01;
        PES_CRC_flag = (v2 >> 1) & 0x01;
        additional_copy_info_flag = (v2 >> 2) & 0x01;
        DSM_trick_mode_flag = (v2 >> 3) & 0x01;
        ES_rate_flag = (v2 >> 4) & 0x01;
        ESCR_flag = (v2 >> 5) & 0x01;
        PTS_DTS_flags = (v2 >> 6) & 0x03;
        data++;
        len--;

        if (len < 1) {
            return 0;
        }
        PES_header_data_length = (uint8_t)*data;
        data++;
        len--;
        const char *pes_header_start = data;
        if (PTS_DTS_flags == 0x02) {//5B
            if (len < 5) {
                return 0;
            }
            // pts = ff_parse_pes_pts(data);
            pts = (int64_t)(*data&0x0e) << 29;
            data += 1;
            pts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff) << 15;
            data += 2;    
            pts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff);
            data += 2;
            len -= 5;

            dts = pts;
        }

        if (PTS_DTS_flags == 0x03) {//5B
            if (len < 10) {
                return 0;
            }
            pts = (int64_t)(*data&0x0e) << 29;
            data += 1;
            pts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff) << 15;
            data += 2;    
            pts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff);
            data += 2;

            dts = (int64_t)(*data&0x0e) << 29;
            data += 1;
            dts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff) << 15;
            data += 2;    
            dts |= (int64_t(ntohs(*((uint16_t*)data))) >> 1 & 0x7fff);
            data += 2;

            len -= 10;
        }

        if (ESCR_flag) {
            if (len < 5) {
                return 0;
            }
            ESCR_base = ((((int64_t)*data) >> 1) & 0x07) << 30;
            data += 1;
            ESCR_base |= (int64_t(*((uint16_t*)data)) >> 1 & 0x7fff) << 15;
            data += 2;    
            ESCR_base |= (int64_t(*((uint16_t*)data)) >> 1 & 0x7fff);
            data += 2;
            len -= 5;
        }

        if (ES_rate_flag) {
            if (len < 3) {
                return 0;
            }

            char *p = (char*)&ES_rate;
            p[0] = data[2];
            p[1] = data[1];
            p[2] = data[0];
            ES_rate = ES_rate>>1;
            data += 3;
            len -= 3;
        }

        if (DSM_trick_mode_flag) {
            if (len < 1) {
                return 0;
            }
            int8_t v1 = *data;
            trick_mode_control = (TrickMode)((v1 >> 5)&0x07);
            if (trick_mode_control == FastForward) {
                field_id = (v1 >> 3) & 0x03;
                intra_slice_refresh = (v1 >> 2) & 0x01;
                frequency_truncation = (v1 >> 0) & 0x03;
            } else if (trick_mode_control == SlowMotion) {
                rep_cntrl = v1 & 0x1f;
            } else if (trick_mode_control == FreezeFrame) {
                field_id = (v1 >> 3) & 0x03;
            } else if (trick_mode_control == FastReverse) {
                field_id = (v1 >> 3) & 0x03;
                intra_slice_refresh = (v1 >> 2) & 0x01;
                frequency_truncation = (v1 >> 0) & 0x03;
            } else if (trick_mode_control == SlowReverse) {
                rep_cntrl = v1 & 0x1f;
            }
            data++;
            len--;
        }

        if (additional_copy_info_flag) {
            if (len < 1) {
                return 0;
            }
            additional_copy_info = *data & 0x3f;
            data++;
            len--;
        }

        if (PES_CRC_flag) {
            if (len < 2) {
                return 0;
            }
            previous_PES_packet_CRC = ntohs(*(int16_t*)data);
            data += 2;
            len -= 2;
        }

        if (PES_extension_flag) {
            if (len < 1) {
                return 0;
            }
            int8_t v1 = *data;
            PES_extension_flag_2 = v1 & 0x01;
            P_STD_buffer_flag = (v1 >> 4) & 0x01;
            program_packet_sequence_counter_flag = (v1 >> 5) & 0x01;
            pack_header_field_flag = (v1 >> 6) & 0x01;
            PES_private_data_flag = (v1 >> 7) & 0x01;
            data++;
            len--;

            if (PES_private_data_flag) {
                if (len < 128) {
                    return 0;
                }
                memcpy(PES_private_data, data, 128);
                data += 128;
                len -= 128;
            }

            if (pack_header_field_flag) {
                if (len < 1) {
                    return 0;
                }
                pack_field_length = *data;
                data++;
                len--;

                if (len < pack_field_length) {
                    return 0;
                }
                data += pack_field_length;
                len -= pack_field_length;
            }

            if (program_packet_sequence_counter_flag) {
                if (len < 2) {
                    return 0;
                }
                program_packet_sequence_counter = *data & 0x7f;
                original_stuff_length = *(data + 1) & 0x3f;
                MPEG1_MPEG2_identifier = (*(data + 1) >> 6)&0x01;
                data += 2;
                len -= 2;
            }

            if (P_STD_buffer_flag) {
                if (len < 2) {
                    return 0;
                }
                int16_t v1 = ntohs(*(int16_t*)data);
                P_STD_buffer_size = v1 & 0x1fff;
                P_STD_buffer_scale = (v1 >> 13) & 0x01;
                data += 2;
                len -= 2;
            }

            if (PES_extension_flag_2) {
                if (len < 1) {
                    return 0;
                }
                PES_extension_field_length = *data;
                data++;
                len--;

                if (len < PES_extension_field_length) {
                    return 0;
                }
                data += PES_extension_field_length;
                len -= PES_extension_field_length;
            }
        }

        int32_t header_consumed = (data - pes_header_start);
        int32_t stuffing_bytes = PES_header_data_length - header_consumed;
        if (stuffing_bytes < 0) {
            return -2;
        }

        if (len < stuffing_bytes) {
            return 0;
        }
        data += stuffing_bytes;
        len -= stuffing_bytes;
        
        //for (i = 0; i < N2; i++) { 
        // PES_packet_data_byte 8 bslbf 
        // }
        // pes_payload = std::string_view(data, pes_data_end - data);
        // CORE_INFO("PES_packet_data1.size():{}, stuffing_bytes:{}, header_consumed:{}", PES_packet_data.size(), stuffing_bytes, header_consumed);
        // data += PES_packet_data.size();
        // len -= PES_packet_data.size();
    } else if (stream_id == TsPESStreamIdProgramStreamMap ||
               stream_id == TsPESStreamIdPrivateStream2 ||
               stream_id == TsPESStreamIdEcmStream ||
               stream_id == TsPESStreamIdEmmStream ||
               stream_id == TsPESStreamIdProgramStreamDirectory ||
               stream_id == TsPESStreamIdDsmccStream ||
               stream_id == TsPESStreamIdH2221TypeE) {
        // if (len < PES_packet_length) {
        //     return 0;
        // }
        // pes_payload = std::string_view(data, PES_packet_length);
        // data += PES_packet_length;
        // len -= PES_packet_length;
    } else if (stream_id == TsPESStreamIdPaddingStream) {
        // if (len < PES_packet_length) {
        //     return 0;
        // }
        // pes_payload = std::string_view(data, PES_packet_length);
        // data += PES_packet_length;
        // len -= PES_packet_length;
    }

    return data - data_start;
}

int32_t PESPacket::decode(const char *data, int32_t len) {
    int32_t header_consumed = pes_header.decode(data, len);
    if (header_consumed < 0) {
        return header_consumed;
    } else if (header_consumed == 0) {
        return 0;
    }
    // pes_header_sv = std::string_view(data, header_consumed);
    pes_payload = std::string_view(data + header_consumed, len - header_consumed);
    return header_consumed + pes_payload.size();
}