#include "rtcp_rr.hpp"
using namespace mms;
RtcpRR::RtcpRR() {
    header_.pt = PT_RTCP_RR;
}

int32_t RtcpRR::decode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    int32_t consumed = header_.decode(data, len);
    if (consumed <= 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;
    for (uint8_t i = 0; i < header_.rc; i++) {
        ReceptionReportBlock reception_report_block;
        consumed = reception_report_block.decode(data, len);
        if (consumed <= 0) {
            return 0;
        }
        data += consumed;
        len -= consumed;
        reception_report_blocks.emplace_back(reception_report_block);
    }
    return data - data_start;
}

void RtcpRR::add_reception_report_block(const ReceptionReportBlock & report) {
    reception_report_blocks.push_back(report);
}

int32_t RtcpRR::encode(uint8_t *data, int32_t len) {
    uint8_t *data_start = data;
    header_.rc = reception_report_blocks.size();
    header_.length = (4 + 24*reception_report_blocks.size())/4;
    int32_t consumed = header_.encode(data, len);
    if (consumed <= 0) {
        return 0;
    }

    data += consumed;
    len -= consumed;

    for (auto & r : reception_report_blocks) {
        consumed = r.encode(data, len);
        if (consumed <= 0) {
            return 0;
        }
        data += consumed;
        len -= consumed;
    }
    return data - data_start;
}
