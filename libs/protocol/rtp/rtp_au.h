#pragma once
#include <stdint.h>
namespace mms {
struct AUConfig {
    uint32_t constant_size;//constant_size和size_length不能同时出现, 一般都是size_length，目前先处理这种情况，constant_size应该是在某种编码模式固定长度的时候用
    uint16_t size_length = 0;//The number of bits on which the AU-size field is encoded in the AU-header. 如果没有这个字段，则au-headers-length字段也不存在
    uint16_t index_length = 0;//The number of bits on which the AU-Index is encoded in the first AU-header
    uint16_t index_delta_length = 0;//The number of bits on which the AU-Index-delta field is encoded in any non-first AU-header.
    uint16_t cts_delta_length;//The number of bits on which the CTS-delta field is encoded in the AU-header.
    uint16_t dts_delta_length;//The number of bits on which the DTS-delta field is encoded in the AU-header.
    uint8_t random_access_indication;//A decimal value of zero or one, indicating whether the RAP-flag is present in the AU-header.
    uint8_t stream_state_indication;//The number of bits on which the Stream-state field is encoded in the AU-header.
    uint16_t auxiliary_data_size_length;//The number of bits that is used to encode the auxiliary-data-size field.
};
// 3.2.1.  The AU Header Section
//    When present, the AU Header Section consists of the AU-headers-length
//    field, followed by a number of AU-headers, see Figure 2.

//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+
//       |AU-headers-length|AU-header|AU-header|      |AU-header|padding|
//       |                 |   (1)   |   (2)   |      |   (n)   | bits  |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+

//                    Figure 2: The AU Header Section
// mark: AU-headers-length 表示后续的AU-header的比特数， 注意：不包括自己，不包括padding

// 3.2.1.1.  The AU-header

//    Each AU-header may contain the fields given in Figure 3.  The length
//    in bits of the fields, with the exception of the CTS-flag, the
//    DTS-flag and the RAP-flag fields, is defined by MIME format
//    parameters; see section 4.1.  If a MIME format parameter has the
//    default value of zero, then the associated field is not present.  The
//    number of bits for fields that are present and that represent the
//    value of a parameter MUST be chosen large enough to correctly encode
//    the largest value of that parameter during the session.

//    If present, the fields MUST occur in the mutual order given in Figure
//    3.  In the general case, a receiver can only discover the size of an
//    AU-header by parsing it since the presence of the CTS-delta and DTS-
//    delta fields is signaled by the value of the CTS-flag and DTS-flag,
//    respectively.

//       +---------------------------------------+
//       |     AU-size                           |
//       +---------------------------------------+
//       |     AU-Index / AU-Index-delta         |
//       +---------------------------------------+
//       |     CTS-flag                          |
//       +---------------------------------------+
//       |     CTS-delta                         |
//       +---------------------------------------+
//       |     DTS-flag                          |
//       +---------------------------------------+
//       |     DTS-delta                         |
//       +---------------------------------------+
//       |     RAP-flag                          |
//       +---------------------------------------+
//       |     Stream-state                      |
//       +---------------------------------------+
struct AUHeader {
    uint16_t AU_size;//au的大小,
    uint16_t AU_index;
    uint16_t AU_index_delta;
    uint8_t CTS_flag;
    uint32_t CTS_delta;
    uint8_t DTS_flag;
    uint32_t DTS_delta;
    uint8_t RAP_flag;
    uint32_t stream_state;
};

};