#pragma once
#include "rtcp_packet.h"
#include "rtcp_sr.h"
#include "rtcp_define.h"
namespace mms {
class RtcpRR : public RtcpPacket {
public:
    RtcpRR();

    void set_ssrc(uint32_t v) {
        header_.sender_ssrc = v;
    }

    int32_t decode(uint8_t *data, int32_t len);
    int32_t encode(uint8_t *data, int32_t len);
    void add_reception_report_block(const ReceptionReportBlock & report);
private:
    std::vector<ReceptionReportBlock> reception_report_blocks;
};
};