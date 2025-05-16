/*
 * @Author: jbl19860422
 * @Date: 2023-12-28 23:56:13
 * @LastEditTime: 2023-12-29 13:40:40
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\rtp\rtp_packer.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
//c
#include <stdint.h>
//stl
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>
#include <list>
#include <string_view>

#include "rtp_packet.h"

namespace mms {
class RtpPacker {
public:
    RtpPacker();
    virtual ~RtpPacker();
    std::vector<std::shared_ptr<RtpPacket>> pack(char *data, int32_t len, uint8_t pt, int ssrc, int64_t pts);
    virtual std::vector<std::shared_ptr<RtpPacket>> pack(const std::list<std::string_view> & nalus, uint8_t pt, int ssrc, int64_t pts);
public:
    uint16_t get_sequence_num();
    std::atomic<uint16_t> rtp_seqnum_{0};
};

class H265RtpPacker : public RtpPacker {
public:
    H265RtpPacker();
    virtual ~H265RtpPacker();
    std::vector<std::shared_ptr<RtpPacket>> pack(const std::list<std::string_view> & nalus, uint8_t pt, int ssrc, int64_t pts) override;
};
};
