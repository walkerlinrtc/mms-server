#pragma once
#include <set>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>

#include "session-level/protocol_version.hpp"
#include "session-level/origin.hpp"
#include "session-level/session_name.hpp"
#include "session-level/session_information.hpp"
#include "session-level/uri.hpp"
#include "session-level/email.hpp"
#include "session-level/phone.hpp"
#include "session-level/connection_info.hpp"
#include "session-level/bundle.hpp"
#include "session-level/timing.hpp"
#include "session-level/tool.hpp"
#include "attribute/common/dir.hpp"
#include "protocol/sdp/ice/ice_ufrag.h"
#include "protocol/sdp/ice/ice_pwd.h"

#include "media-level/media_sdp.hpp"
#include "webrtc/ssrc.h"

namespace mms {
struct Sdp {
public:
    int32_t parse(const std::string & sdp);
    int get_version();
    void set_version(int v);

    void set_origin(const Origin &origin) {
        origin_ = origin;
    }

    const Origin & get_origin() const {
        return origin_;
    }

    void set_session_name(const SessionName & session_name) {
        session_name_ = session_name;
    }

    SessionName & get_session_name() {
        return session_name_;
    }

    void set_time(const Timing & time) {
        time_ = time;
    }

    const Timing & get_time() {
        return time_;
    }

    void set_tool(const ToolAttr & tool) {
        tool_ = tool;
    }

    const ToolAttr & get_tool() {
        return tool_.value();
    }

    void set_bundle(const BundleAttr & bundle) {
        bundle_attr_ = bundle;
    }

    const BundleAttr & get_bundle() {
        return bundle_attr_.value();
    }

    std::vector<MediaSdp> & get_media_sdps() {
        return media_sdps_;
    }

    void set_media_sdps(const std::vector<MediaSdp> & media_sdps) {
        media_sdps_ = media_sdps;
    }

    void add_media_sdp(const MediaSdp & media_sdp) {
        media_sdps_.emplace_back(media_sdp);
    }

    void add_attr(const std::string & val) {
        attrs_.emplace_back(val);
    }
    
    const std::optional<IceUfrag> & get_ice_ufrag() const {
        if (ice_ufrag_) {
            return ice_ufrag_;
        }

        for (auto & media : media_sdps_) {
            return media.get_ice_ufrag();
        }
        static std::optional<IceUfrag> nullopt = std::nullopt;
        return nullopt;
    }

    const std::optional<IcePwd> & get_ice_pwd() const {
        if (ice_pwd_) {
            return ice_pwd_;
        }

        for (auto & media : media_sdps_) {
            return media.get_ice_pwd();
        }
        static std::optional<IcePwd> nullopt = std::nullopt;
        return nullopt;
    }

    std::string to_string() const;
protected:
    ProtocolVersion protocol_version_;
    Origin          origin_;
    SessionName     session_name_;
    Timing          time_;
    std::vector<std::string> attrs_;
    
    std::optional<SessionInformation> session_info_;
    std::optional<ToolAttr> tool_;
    std::optional<DirAttr> dir_;
    std::optional<Uri> uri_;
    std::optional<EmailAddress> email_;
    std::optional<Phone> phone_;
    std::optional<ConnectionInfo> conn_info_;
    std::optional<IceUfrag> ice_ufrag_;
    std::optional<IcePwd> ice_pwd_;
    std::optional<BundleAttr> bundle_attr_;
    
    std::vector<MediaSdp> media_sdps_;
};
};