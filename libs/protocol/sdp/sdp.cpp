#include "spdlog/spdlog.h"

#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "sdp.hpp"
#include "base/utils/utils.h"

using namespace mms;
int32_t Sdp::parse(const std::string &sdp)
{
    std::vector<std::string> lines;
    boost::split(lines, sdp, boost::is_any_of("\n"));
    if (lines.size() <= 3)
    {
        return -1;
    }
    // protocol version
    if (!boost::starts_with(lines[0], ProtocolVersion::prefix))
    {
        return -1;
    }

    if (!protocol_version_.parse(lines[0]))
    {
        return -2;
    }
    // origin
    if (!boost::starts_with(lines[1], Origin::prefix))
    {
        return -3;
    }
    if (!origin_.parse(lines[1]))
    {
        return -4;
    }
    // session_name
    if (!boost::starts_with(lines[2], SessionName::prefix))
    {
        return -5;
    }

    if (!session_name_.parse(lines[2]))
    {
        return -6;
    }

    size_t i = 3;
    while (!boost::starts_with(lines[i], MediaSdp::prefix))
    { //在m=之前，都是session的属性
        if (DirAttr::is_my_prefix(lines[i]))
        {
            DirAttr dir;
            if (!dir.parse(lines[i]))
            {
                return -7;
            }
            dir_ = dir;
        }
        else if (boost::starts_with(lines[i], Uri::prefix))
        {
            Uri uri;
            if (!uri.parse(lines[i]))
            {
                return -10;
            }
            uri_ = uri;
        }
        else if (boost::starts_with(lines[i], EmailAddress::prefix))
        {
            EmailAddress email;
            if (!email.parse(lines[i]))
            {
                return -11;
            }
            email_ = email;
        }
        else if (boost::starts_with(lines[i], Phone::prefix))
        {
            Phone phone;
            if (!phone.parse(lines[i]))
            {
                return -12;
            }
            phone_ = phone;
        }
        else if (boost::starts_with(lines[i], ConnectionInfo::prefix))
        {
            ConnectionInfo conn_info;
            if (!conn_info.parse(lines[i]))
            {
                return -13;
            }
            conn_info_ = conn_info;
        }
        else if (boost::starts_with(lines[i], SessionInformation::prefix))
        {
            SessionInformation si;
            if (!si.parse(lines[i]))
            {
                return -9;
            }
            session_info_ = si;
        }
        else if (boost::starts_with(lines[i], Timing::prefix))
        {
            if (!time_.parse(lines[i]))
            {
                return -10;
            }
        }
        else if (boost::starts_with(lines[i], ConnectionInfo::prefix))
        {
            ConnectionInfo conn_info;
            if (!conn_info.parse(lines[i]))
            {
                return -13;
            }
            conn_info_ = conn_info;
        }
        else if (boost::starts_with(lines[i], IceUfrag::prefix))
        {
            IceUfrag ice_ufrag;
            if (!ice_ufrag.parse(lines[i]))
            {
                return -14;
            }
            ice_ufrag_ = ice_ufrag;
        }
        else if (boost::starts_with(lines[i], IcePwd::prefix))
        {
            IcePwd ice_pwd;
            if (!ice_pwd.parse(lines[i]))
            {
                return -15;
            }
            ice_pwd_ = ice_pwd;
        }

        i++;
    }

    std::optional<MediaSdp> curr_media_sdp;
    for (; i < lines.size(); i++)
    {
        if (boost::starts_with(lines[i], MediaSdp::prefix))
        {
            if (curr_media_sdp.has_value())
            {
                media_sdps_.emplace_back(curr_media_sdp.value());
            }

            MediaSdp msdp;
            if (!msdp.parse(lines[i]))
            {
                return -7;
            }
            curr_media_sdp = msdp;
            continue;
        }

        bool ret = false;
        if (curr_media_sdp.has_value())
        {
            ret = (*curr_media_sdp).parse_attr(lines[i]);
        }

        if (!ret)
        {
            if (boost::starts_with(lines[i], BundleAttr::prefix))
            {
                BundleAttr bundle_attr;
                if (!bundle_attr.parse(lines[i]))
                {
                    return -14;
                }
                bundle_attr_ = bundle_attr;
            }
        }
    }

    if (curr_media_sdp.has_value())
    {
        media_sdps_.emplace_back(curr_media_sdp.value());
    }

    return 0;
}

int Sdp::get_version()
{
    return protocol_version_.get_version();
}

void Sdp::set_version(int v)
{
    protocol_version_.set_version(v);
}

std::string Sdp::to_string() const
{
    std::ostringstream oss;
    oss << protocol_version_.to_string();
    oss << origin_.to_string();
    oss << session_name_.to_string();
    oss << time_.to_string();
    if (session_info_)
    {
        oss << session_info_.value().to_string();
    }

    if (tool_)
    {
        oss << tool_.value().to_string();
    }

    for (auto &a : attrs_)
    {
        oss << "a=" << a << std::endl;
    }

    if (ice_ufrag_)
    {
        oss << ice_ufrag_.value().to_string();
    }

    if (ice_pwd_)
    {
        oss << ice_pwd_.value().to_string();
    }

    if (bundle_attr_)
    {
        oss << bundle_attr_.value().to_string();
    }

    for (auto &m : media_sdps_)
    {
        oss << m.to_string();
    }
    // std::optional<ToolAttr> tool_;
    // std::optional<DirAttr> dir_;
    // std::optional<Uri> uri_;
    // std::optional<EmailAddress> email_;
    // std::optional<Phone> phone_;
    // std::optional<ConnectionInfo> conn_info_;
    // std::optional<BundleAttr> bundle_attr_;

    // std::vector<MediaSdp> media_sdps_;
    // std::set<std::string> candidates_;
    // std::unordered_map<uint32_t, Ssrc> ssrcs_;

    return oss.str();
}