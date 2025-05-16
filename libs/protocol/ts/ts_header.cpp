#include "ts_header.hpp"
using namespace mms;

int32_t TsHeader::encode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    data[0] = sync_byte;
    data++;
    len--;

    uint16_t v;
    v = transport_error_indicator & 0x01;
    v = (payload_unit_start_indicator << 1) & 0x02;
    v = (transport_priority << 2) & 0x04;
    v = (pid & 0x1fff) << 3;
    if (len < 2) {
        return -2;
    }
    *((uint16_t *)data) = v;
    data += 2;
    len -= 2;

    if (len < 1) {
        return -3;
    }
    uint8_t v8 = 0;
    v8 = transport_scrambing_control & 0x03;
    v8 = (adaptation_field_control & 0x03) << 2;
    v8 = (continuity_counter & 0xf) << 4;
    data[0] = v8;
    data++;
    len--;
    return data - data_start;
}

int32_t TsHeader::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    if (len < 1) {
        return -1;
    }
    sync_byte = data[0];
    data++;
    len--;

    if (len < 2) {
        return -2;
    }
    uint16_t v;
    v = *(uint16_t *)data;
    transport_error_indicator = v & 0x01;
    payload_unit_start_indicator = (v >> 1) & 0x01;
    transport_priority = (v >> 2) & 0x01;
    pid = (v >> 3) & 0x1fff;
    data += 2;
    len -= 2;

    if (len < 1) {
        return -3;
    }
    transport_scrambing_control = (E_TsScrambled)(data[0] & 0x03);
    adaptation_field_control = (E_TsAdapationControl)((data[0] >> 2) & 0x03);
    continuity_counter = (data[0] >> 4) & 0x0f;
    return data - data_start;
}

int32_t TsHeader::size() {
    return 4;
}