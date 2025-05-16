#include <sstream>
#include <boost/algorithm/string.hpp>
#include "media_sdp.hpp"
#include "rtcp_fb.h"
#include <iostream>
#include "base/utils/utils.h"
#include "spdlog/spdlog.h"

using namespace mms;
std::string MediaSdp::prefix = "m=";
bool MediaSdp::parse(const std::string &line)
{
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos)
    {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 3)
    {
        return false;
    }

    media = vs[0];
    std::string &sport = vs[1];
    proto = vs[2];
    for (size_t i = 3; i < vs.size(); i++)
    {
        fmts.emplace_back(std::atoi(vs[i].c_str()));
    }

    boost::split(vs, sport, boost::is_any_of("/"));
    try
    {
        if (vs.size() > 1)
        {
            port = std::atoi(vs[0].c_str());
            port_count = std::atoi(vs[1].c_str());
        }
        else
        {
            port = std::atoi(vs[0].c_str());
        }
    }
    catch (std::exception &e)
    {
        return false;
    }

    return true;
}

bool MediaSdp::parse_attr(const std::string &line)
{
    if (boost::starts_with(line, IceUfrag::prefix))
    {
        IceUfrag ice_ufrag;
        if (!ice_ufrag.parse(line))
        {
            return false;
        }
        ice_ufrag_ = ice_ufrag;
        return true;
    }
    else if (boost::starts_with(line, IcePwd::prefix))
    {
        IcePwd ice_pwd;
        if (!ice_pwd.parse(line))
        {
            return false;
        }
        ice_pwd_ = ice_pwd;
        return true;
    }
    else if (boost::starts_with(line, IceOption::prefix))
    {
        IceOption ice_option;
        if (!ice_option.parse(line))
        {
            return false;
        }
        ice_option = ice_option;
        return true;
    }
    else if (boost::starts_with(line, Extmap::prefix))
    {
        Extmap ext_map;
        if (!ext_map.parse(line))
        {
            return false;
        }
        ext_maps.emplace_back(ext_map);
    }
    else if (DirAttr::is_my_prefix(line))
    {
        DirAttr d;
        if (!d.parse(line))
        {
            return false;
        }
        dir = d;
    }
    else if (boost::starts_with(line, ConnectionInfo::prefix))
    {
        ConnectionInfo ci;
        if (!ci.parse(line))
        {
            return false;
        }
        connection_info = ci;
    }
    else if (boost::starts_with(line, MidAttr::prefix))
    {
        MidAttr ma;
        if (!ma.parse(line))
        {
            return false;
        }
        mid = ma;
    }
    else if (boost::starts_with(line, BandWidth::prefix)) 
    {
        if (!bw_.parse(line))
        {
            return false;
        }
    }
    else if (Payload::is_my_prefix(line))
    {
        Payload payload;
        if (!payload.parse(line))
        {
            return false;
        }
        curr_pt = payload.get_pt();
        payloads_.insert(std::pair(payload.get_pt(), payload));
    }
    else if (RtcpFb::is_my_prefix(line))
    {
        if (0 != payloads_[curr_pt].parse_rtcp_fb_attr(line))
        {
            return false;
        }
    }
    else if (Fmtp::is_my_prefix(line))
    {
        if (0 != payloads_[curr_pt].parse_fmtp_attr(line))
        {
            return false;
        }
    }
    else if (boost::starts_with(line, MaxPTimeAttr::prefix))
    {
        MaxPTimeAttr attr;
        if (!attr.parse(line))
        {
            return false;
        }
        max_ptime = attr;
    }
    else if (boost::starts_with(line, Control::prefix))
    {
        Control ctrl;
        if (!ctrl.parse(line)) 
        {
            return false;
        }
        control = ctrl;
    }
    else if (boost::starts_with(line, Ssrc::prefix))
    {
        uint32_t id = Ssrc::parse_id_only(line);
        Ssrc & ssrc = ssrcs_[id];
        if (!ssrc.parse(line))
        {
            return false;
        }
    }
    else if (boost::starts_with(line, SetupAttr::prefix))
    {
        SetupAttr setup;
        if (!setup.parse(line))
        {
            return false;
        }
        setup_ = setup;
    }
    else if (boost::starts_with(line, SsrcGroup::prefix))
    {
        SsrcGroup ssrc_group;
        if (!ssrc_group.parse(line))
        {
            return false;
        }
        ssrc_group_ = ssrc_group;
    }
    return true;
}

std::string MediaSdp::to_string() const
{
    std::ostringstream oss;
    {
        oss << prefix << media << " " << port << " " << proto;
        for (size_t i = 0; i < fmts.size(); i++)
        {
            if (i != fmts.size() - 1)
            {
                oss << " " << std::to_string(fmts[i]) << " ";
            }
            else
            {
                oss << " " << std::to_string(fmts[i]);
            }
        }
        oss << std::endl;
    }

    if (connection_info)
    {
        oss << connection_info.value().to_string();
    }

    if (ice_ufrag_)
    {
        oss << ice_ufrag_.value().to_string();
    }

    if (ice_pwd_)
    {
        oss << ice_pwd_.value().to_string();
    }

    if (ice_option_)
    {
        oss << ice_option_.value().to_string();
    }

    for (auto &c : candidates_)
    {
        oss << c.to_string();
    }

    if (rtcp_mux_)
    {
        oss << rtcp_mux_.value().to_string();
    }

    for (auto &p : payloads_)
    {
        oss << p.second.to_string();
    }

    if (dir) {
        oss << dir.value().to_string();
    }
    
    if (setup_) {
        oss << setup_.value().to_string();
    }
    
    if (mid) {
        oss << mid.value().to_string();
    }
    
    if (fingerprint_) {
        oss << fingerprint_.value().to_string();
    }

    if (control) {
        oss << control.value().to_string();
    }
    

    if (ssrc_group_.has_value())
    {
        oss << ssrc_group_.value().to_string();
    }

    for (auto & p : ssrcs_)
    {
        oss << p.second.to_string();
    }

    return oss.str();
}