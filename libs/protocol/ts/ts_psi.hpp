#pragma once
#include <stdint.h>

namespace mms {
/**
* 2.4.4.4 Table_id assignments, hls-mpeg-ts-iso13818-1.pdf, page 62
* The table_id field identifies the contents of a Transport Stream PSI section as shown in Table 2-26.
 */
enum TsPsiTableId {
	// program_association_section
	TsPsiTableIdPas = 0x00,
	// conditional_access_section (CA_section)
	TsPsiTableIdCas = 0x01,
	// TS_program_map_section
	TsPsiTableIdPms = 0x02,
	// TS_description_section
	TsPsiTableIdDs = 0x03,
	// ISO_IEC_14496_scene_description_section
	TsPsiTableIdSds = 0x04,
	// ISO_IEC_14496_object_descriptor_section
	TsPsiTableIdOds = 0x05,
	// ITU-T Rec. H.222.0 | ISO/IEC 13818-1 reserved
	TsPsiIdTableIso138181Start = 0x06,
	TsPsiIdTableIso138181End   = 0x37,
	// Defined in ISO/IEC 13818-6
	TsPsiIdTableIso138186Start = 0x38,
	TsPsiIdTableIso138186End   = 0x3F,
	// User private
	TsPsiTableIdUserStart = 0x40,
	TsPsiTableIdUserEnd   = 0xFE,
	// forbidden
	TsPsiTableIdForbidden = 0xFF
};

class TsPayloadPSI {
public:
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
public:
    int32_t encode(uint8_t *data, int32_t len);
	int32_t decode(uint8_t *data, int32_t len);
	int32_t size();
};



};