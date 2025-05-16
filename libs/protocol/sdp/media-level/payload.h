#pragma once
#include <string>
#include <vector>
#include "rtcp_fb.h"
#include "fmtp.h"
#include "bandwidth.hpp"

namespace mms
{
    struct Payload
    {
    public:
        static std::string prefix;
        static bool is_my_prefix(const std::string &line);
        bool parse(const std::string &line);
        Payload() = default;
        Payload(uint32_t payload_type, const std::string &encoding_name, uint32_t clock_rate, const std::vector<std::string> &encoding_params) : payload_type_(payload_type), encoding_name_(encoding_name), clock_rate_(clock_rate), encoding_params_(encoding_params)
        {
        }
        bool parse_rtcp_fb_attr(const std::string &line);
        bool parse_fmtp_attr(const std::string &line);

        const std::vector<RtcpFb> &get_rtcp_fbs()
        {
            return rtcp_fbs_;
        }

        void set_rtcp_fbs(const std::vector<RtcpFb> &rtcp_fbs)
        {
            rtcp_fbs_ = rtcp_fbs;
        }

        void add_rtcp_fb(const RtcpFb &rtcp_fb)
        {
            rtcp_fbs_.push_back(rtcp_fb);
        }

        const std::string &get_encoding_name() const
        {
            return encoding_name_;
        }

        uint32_t get_clock_rate() const
        {
            return clock_rate_;
        }

        uint32_t get_pt() const
        {
            return payload_type_;
        }

        void set_pt(uint32_t pt)
        {
            payload_type_ = pt;
        }

        void add_fmtp(const Fmtp &fmtp)
        {
            fmtps_.insert(std::pair(fmtp.get_pt(), fmtp));
        }

        void set_fmtps(const std::unordered_map<uint32_t, Fmtp> & fmtps) {
            fmtps_ = fmtps;
        }

        const std::unordered_map<uint32_t, Fmtp> & get_fmtps() const
        {
            return fmtps_;
        }

        const std::vector<std::string> & get_encoding_params() const {
            return encoding_params_;
        }

        std::string to_string() const;
    private:
        uint32_t payload_type_;
        std::string encoding_name_;
        uint32_t clock_rate_;
        std::vector<std::string> encoding_params_;
        std::vector<RtcpFb> rtcp_fbs_;
        std::unordered_map<uint32_t, Fmtp> fmtps_;
    };
};