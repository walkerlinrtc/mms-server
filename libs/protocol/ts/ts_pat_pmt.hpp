#pragma once
#include <arpa/inet.h>

#include <vector>
#include <memory>
#include "ts_psi.hpp"

namespace mms {
// the mpegts header specifed the video/audio pid.
#define TS_PMT_NUMBER 1
#define TS_PMT_PID 0x1001
#define TS_VIDEO_AVC_PID 0x100
#define TS_VIDEO_HEVC_PID 0x100

#define TS_AUDIO_AAC_PID 0x101
#define TS_AUDIO_MP3_PID 0x101

// The pid of ts packet,
// Table 2-3 - PID table, hls-mpeg-ts-iso13818-1.pdf, page 37
// NOTE - The transport packets with PID values 0x0000, 0x0001, and 0x0010-0x1FFE are allowed to carry a PCR.
enum TsPid
{
    // Program Association Table(see Table 2-25).
    TsPidPAT = 0x00,
    // Conditional Access Table (see Table 2-27).
    TsPidCAT = 0x01,
    // Transport Stream Description Table
    TsPidTSDT = 0x02,
    // Reserved
    TsPidReservedStart = 0x03,
    TsPidReservedEnd = 0x0f,
    // May be assigned as network_PID, Program_map_PID, elementary_PID, or for other purposes
    TsPidAppStart = 0x10,
    TsPidAppEnd = 0x1ffe,
    // For null packets (see Table 2-3)
    TsPidNULL = 0x01FFF,
};

// The stream_id of PES payload of ts packet.
// Table 2-18 - Stream_id assignments, hls-mpeg-ts-iso13818-1.pdf, page 52.
enum TsPESStreamId
{
    // program_stream_map
    TsPESStreamIdProgramStreamMap = 0xbc, // 0b10111100
    // private_stream_1
    TsPESStreamIdPrivateStream1 = 0xbd, // 0b10111101
    // padding_stream
    TsPESStreamIdPaddingStream = 0xbe, // 0b10111110
    // private_stream_2
    TsPESStreamIdPrivateStream2 = 0xbf, // 0b10111111
    
    // 110x xxxx
    // ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC
    // 14496-3 audio stream number x xxxx
    // ((sid >> 5) & 0x07) == TsPESStreamIdAudio
    // @remark, use TsPESStreamIdAudioCommon as actually audio, TsPESStreamIdAudio to check whether audio.
    TsPESStreamIdAudioChecker = 0x06, // 0b110
    TsPESStreamIdAudioCommon = 0xc0,
    
    // 1110 xxxx
    // ITU-T Rec. H.262 | ISO/IEC 13818-2 or ISO/IEC 11172-2 or ISO/IEC
    // 14496-2 video stream number xxxx
    // ((stream_id >> 4) & 0x0f) == TsPESStreamIdVideo
    // @remark, use TsPESStreamIdVideoCommon as actually video, TsPESStreamIdVideo to check whether video.
    TsPESStreamIdVideoChecker = 0x0e, // 0b1110
    TsPESStreamIdVideoCommon = 0xe0,
    
    // ECM_stream
    TsPESStreamIdEcmStream = 0xf0, // 0b11110000
    // EMM_stream
    TsPESStreamIdEmmStream = 0xf1, // 0b11110001
    // DSMCC_stream
    TsPESStreamIdDsmccStream = 0xf2, // 0b11110010
    // 13522_stream
    TsPESStreamId13522Stream = 0xf3, // 0b11110011
    // H_222_1_type_A
    TsPESStreamIdH2221TypeA = 0xf4, // 0b11110100
    // H_222_1_type_B
    TsPESStreamIdH2221TypeB = 0xf5, // 0b11110101
    // H_222_1_type_C
    TsPESStreamIdH2221TypeC = 0xf6, // 0b11110110
    // H_222_1_type_D
    TsPESStreamIdH2221TypeD = 0xf7, // 0b11110111
    // H_222_1_type_E
    TsPESStreamIdH2221TypeE = 0xf8, // 0b11111000
    // ancillary_stream
    TsPESStreamIdAncillaryStream = 0xf9, // 0b11111001
    // SL_packetized_stream
    TsPESStreamIdSlPacketizedStream = 0xfa, // 0b11111010
    // FlexMux_stream
    TsPESStreamIdFlexMuxStream = 0xfb, // 0b11111011
    // reserved data stream
    // 1111 1100 ... 1111 1110
    // program_stream_directory
    TsPESStreamIdProgramStreamDirectory = 0xff, // 0b11111111
};

// Table 2-29 - Stream type assignments
enum TsStream
{
    // ITU-T | ISO/IEC Reserved
    TsStreamReserved = 0x00,
    TsStreamForbidden = 0xff,
    // ISO/IEC 11172 Video
    // ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
    // ISO/IEC 11172 Audio
    // ISO/IEC 13818-3 Audio
    TsStreamAudioMp3 = 0x04,
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
    // ISO/IEC 13522 MHEG
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
    // ITU-T Rec. H.222.1
    // ISO/IEC 13818-6 type A
    // ISO/IEC 13818-6 type B
    // ISO/IEC 13818-6 type C
    // ISO/IEC 13818-6 type D
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
    // ISO/IEC 13818-7 Audio with ADTS transport syntax
    TsStreamAudioAAC = 0x0f,
    // ISO/IEC 14496-2 Visual
    TsStreamVideoMpeg4 = 0x10,
    // ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
    TsStreamAudioMpeg4 = 0x11,
    // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
    // ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
    // ISO/IEC 13818-6 Synchronized Download Protocol
    // ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
    // 0x15-0x7F
    TsStreamVideoH264 = 0x1b,
    TsStreamVideoH265 = 0x24,
    // User Private
    // 0x80-0xFF
    TsStreamAudioAC3 = 0x81,
    TsStreamAudioDTS = 0x8a,
};

class TsPayloadPATProgram {
public:
    TsPayloadPATProgram(uint16_t n, uint16_t p) {
        number = n;
        pid = p;
    }

    int32_t encode(char *data, size_t len) {
        uint32_t tmpv = pid & 0x1FFF;
        tmpv |= (uint32_t(const1_value) << 13) & 0xE000;
        tmpv |= (uint32_t(number) << 16) & 0xFFFF0000;
        if (len < 4) {
            return -1;
        }
        *(uint32_t*)data = htonl(tmpv);
        return 4;
    }

    int32_t decode(char *data, size_t len) {
        if (len < 4) {
            return -1;
        }
        uint32_t tmpv = ntohl(*(uint32_t*)data);
        pid = tmpv & 0x1FFF;
        const1_value = (tmpv >> 13) & 0x3;
        number = (tmpv >> 16) & 0xFFFF;
        return 4;
    }
public:
    /**
	 * Program_number is a 16-bit field. It specifies the program to which the program_map_PID is
	 * applicable. When set to 0x0000, then the following PID reference shall be the network PID. For all other cases the value
	 * of this field is user defined. This field shall not take any single value more than once within one version of the Program
	 * Association Table.
	 */
    uint16_t number;//16bit
    /**
	 * reverved value, must be '1'
	 */
    uint8_t const1_value = 0x07;//3bits
    /**
	 * program_map_PID/network_PID 13bits
	 * network_PID - The network_PID is a 13-bit field, which is used only in conjunction with the value of the
	 * program_number set to 0x0000, specifies the PID of the Transport Stream packets which shall contain the Network
	 * Information Table. The value of the network_PID field is defined by the user, but shall only take values as specified in
	 * Table 2-3. The presence of the network_PID is optional.
	 */
    uint16_t pid;//13bits
public:

};

class TsPayloadPAT {
public:
	//psi 信息
	/**
	 * This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
	 * until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
	 * the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
	 * a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
	 * byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
	 * Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
	 * payload of that packet.
	 */
    uint8_t pointer_field;
	// 1B
	/**
	 * This is an 8-bit field, which shall be set to 0x00 as shown in Table 2-26.
	 */
    TsPsiTableId table_id;//PAT表固定为0x00,PMT表为0x02
	/*
	 The section_syntax_indicator is a 1-bit field which shall be set to '1'.
	*/
	uint8_t section_syntax_indicator;//固定为二进制1
	/**
	 * const value, must be '0'
	 */
    uint8_t const0_value;//1bit
	/**
	 * reverved value, must be '1'
	 */
    uint8_t const1_value0;//2bits
	/*
	 This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the number
	 of bytes of the section, starting immediately following the section_length field, and including the CRC. The value in this
	 field shall not exceed 1021 (0x3FD).
	*/
    uint16_t section_length;//12bits 后面数据的长度
    /*
		This is a 16-bit field which serves as a label to identify this Transport Stream from any other
		multiplex within a network. Its value is defined by the user
	*/
    uint16_t transport_streamid;//固定为0x0001
	uint8_t const1_value1;//2bits
    /*
		This 5-bit field is the version number of the whole Program Association Table. The version number
		shall be incremented by 1 modulo 32 whenever the definition of the Program Association Table changes. When the
		current_next_indicator is set to '1', then the version_number shall be that of the currently applicable Program Association
		Table. When the current_next_indicator is set to '0', then the version_number shall be that of the next applicable Program
		Association Table.
	*/
    uint8_t version_number;//5bits 版本号，固定为二进制00000，如果PAT有变化则版本号加1
    /*
		A 1-bit indicator, which when set to '1' indicates that the Program Association Table sent is
		currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable and shall be the next
		table to become valid.
	*/
	uint8_t current_next_indicator;//固定为二进制1，表示这个PAT表可以用，如果为0则要等待下一个PAT表
    /*
		This 8-bit field gives the number of this section. The section_number of the first section in the
		Program Association Table shall be 0x00. It shall be incremented by 1 with each additional section in the Program
		Association Table.
	*/
	uint8_t section_number;//固定为0x00
    /*
		This 8-bit field specifies the number of the last section (that is, the section with the highest
		section_number) of the complete Program Association Table.
	*/
	uint8_t last_section_number;//固定为0x00
    // multiple 4B program data.
	std::vector<TsPayloadPATProgram> programs;
    /**
	 * This is a 32-bit field that contains the CRC value that gives a zero output of the registers in the decoder
	 * defined in Annex A after processing the entire section.
	 * @remark crc32(bytes without pointer field, before crc32 field)
	 */
	uint32_t crc32;
};

struct TsPayloadPMTESInfo {
	/*
		This is an 8-bit field specifying the type of program element carried within the packets with the PID
		whose value is specified by the elementary_PID. The values of stream_type are specified in Table 2-29.
	*/
	TsStream stream_type;//流类型，标志是Video还是Audio还是其他数据，h.264编码对应0x1b，aac编码对应0x0f，mp3编码对应0x03
	uint8_t const1_value0;//3bits
	/*
		This is a 13-bit field specifying the PID of the Transport Stream packets which carry the associated
		program element
	*/
	uint16_t elementary_pid;//13bits
	uint8_t const1_value1;//4bits
	/*
		This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the number
		of bytes of the descriptors of the associated program element immediately following the ES_info_length field.
	*/
	uint16_t es_info_length;//12bits
	std::unique_ptr<uint8_t[]> es_info;
};


class TsPayloadPMT {
public:
	//psi 信息
	/**
	 * This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
	 * until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
	 * the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
	 * a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
	 * byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
	 * Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
	 * payload of that packet.
	 */
    uint8_t pointer_field;
	// 1B
	/**
	 * This is an 8-bit field, which shall be set to 0x00 as shown in Table 2-26.
	 */
    TsPsiTableId table_id;//PAT表固定为0x00,PMT表为0x02
	/*
	 The section_syntax_indicator is a 1-bit field which shall be set to '1'.
	*/
	uint8_t section_syntax_indicator;//固定为二进制1
	/**
	 * const value, must be '0'
	 */
    uint8_t const0_value;//1bit
	/**
	 * reverved value, must be '1'
	 */
    uint8_t const1_value0;//2bits
	/*
	 This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the number
	 of bytes of the section, starting immediately following the section_length field, and including the CRC. The value in this
	 field shall not exceed 1021 (0x3FD).
	*/
    uint16_t section_length;//12bits 后面数据的长度
    /*
		program_number is a 16-bit field. It specifies the program to which the program_map_PID is
		applicable. One program definition shall be carried within only one TS_program_map_section. This implies that a
		program definition is never longer than 1016 (0x3F8). See Informative Annex C for ways to deal with the cases when
		that length is not sufficient. The program_number may be used as a designation for a broadcast channel, for example. By
		describing the different program elements belonging to a program, data from different sources (e.g. sequential events)
		can be concatenated together to form a continuous set of streams using a program_number. For examples of applications
		refer to Annex C.
	*/
	uint16_t program_number;//频道号码，表示当前的PMT关联到的频道，取值0x0001

	// 1B
	/**
	 * reverved value, must be '1'
	 */
	uint8_t const1_value1;//2bits
	/*
		This 5-bit field is the version number of the TS_program_map_section. The version number shall be
		incremented by 1 modulo 32 when a change in the information carried within the section occurs. Version number refers
		to the definition of a single program, and therefore to a single section. When the current_next_indicator is set to '1', then
		the version_number shall be that of the currently applicable TS_program_map_section. When the current_next_indicator
		is set to '0', then the version_number shall be that of the next applicable TS_program_map_section.
	*/
	uint8_t version_number;//5bits 版本号，固定为00000，如果PAT有变化则版本号加1
	/*
		A 1-bit field, which when set to '1' indicates that the TS_program_map_section sent is
		currently applicable. When the bit is set to '0', it indicates that the TS_program_map_section sent is not yet applicable
		and shall be the next TS_program_map_section to become valid.
	*/
	uint8_t current_next_indicator;//1bit	固定为1就好，没那么复杂
	/*
		The value of this 8-bit field shall be 0x00
	*/
	uint8_t section_number;//
	/*
		The value of this 8-bit field shall be 0x00.
	*/
	uint8_t last_section_number;//8bit
	uint8_t const1_value2;//3bits
	/*
		This is a 13-bit field indicating the PID of the Transport Stream packets which shall contain the PCR fields
		valid for the program specified by program_number. If no PCR is associated with a program definition for private
		streams, then this field shall take the value of 0x1FFF. Refer to the semantic definition of PCR in 2.4.3.5 and Table 2-3
		for restrictions on the choice of PCR_PID value
	*/
	uint16_t PCR_PID;//13bits
	// 2B
	uint8_t const1_value3;//4bits
	/*
		This is a 12-bit field, the first two bits of which shall be '00'. The remaining 10 bits specify the
		number of bytes of the descriptors immediately following the program_info_length field.
	*/
	uint16_t program_info_length;//12bits
	std::unique_ptr<uint8_t[]> program_descriptor;//
	std::vector<TsPayloadPMTESInfo> infoes;
};

};