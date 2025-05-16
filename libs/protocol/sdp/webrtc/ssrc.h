#pragma once
#include <string>
namespace mms {
struct Ssrc {
public:
    static std::string prefix;
    Ssrc() {};
    Ssrc& operator=(const Ssrc &) = default;
    Ssrc(const Ssrc & s) {
        id_ = s.id_;
        cname_ = s.cname_;
        mslabel_ = s.mslabel_;
        label_ = s.label_;
    }

    Ssrc(uint32_t id, const std::string & cname, const std::string & mslabel, const std::string & label) {
        id_ = id;
        cname_ = cname;
        mslabel_ = mslabel;
        label_ = label;
    }

    static uint32_t parse_id_only(const std::string & line);
    bool parse(const std::string & line);

    uint32_t get_id() const {
        return id_;
    }

    void set_id(uint32_t val) {
        id_ = val;
    }

    const std::string & get_cname() const {
        return cname_;
    }

    void set_cname(const std::string & val) {
        cname_ = val;
    }

    const std::string & get_mslabel() const {
        return mslabel_;
    }

    void set_mslabel(const std::string & val) {
        mslabel_ = val;
    }

    const std::string & get_label() const {
        return label_;
    }

    void set_label(const std::string & val) {
        label_ = val;
    }

    std::string to_string() const;
private:
    uint32_t id_;
    std::string cname_;
    std::string mslabel_;
    std::string label_;
};
};