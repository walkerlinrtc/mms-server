#include <sstream>
#include <boost/algorithm/string.hpp>

#include "http_request.hpp"
#include "base/network/socket_interface.hpp"

using namespace mms;

HttpRequest::HttpRequest() {

}

HttpRequest::~HttpRequest() {
}


HTTP_METHOD HttpRequest::get_method() const {
    return method_;
}

void HttpRequest::set_method(HTTP_METHOD val) {
    method_ = val;
    method_str_ = method_map_[val];
}

void HttpRequest::set_method(const std::string & val) {
    for (auto & p : method_map_) {
        if (p.second == val) {
            method_ = p.first;
            method_str_ = val;
            break;
        }
    }
}

const std::string & HttpRequest::get_path() const {
    return path_;
}

void HttpRequest::set_path(const std::string & path) {
    path_ = path;
}

const std::string & HttpRequest::get_header(const std::string & k) const {
    static std::string empty_str("");
    auto it = headers_.find(k);
    if (it == headers_.end()) {
        return empty_str;
    }
    return it->second;
}

void HttpRequest::add_header(const std::string & k, const std::string & v) {
    headers_[k] = v;
}

const std::string & HttpRequest::get_query_param(const std::string & k) const {
    static std::string empty_str("");
    auto it = query_params_.find(k);
    if (it == query_params_.end()) {
        return empty_str;
    }
    return it->second;
}

const std::unordered_map<std::string, std::string> & HttpRequest::get_query_params() const {
    return query_params_;
}

void HttpRequest::add_query_param(const std::string & k, const std::string & v) {
    query_params_[k] = v;
}

void HttpRequest::set_query_params(const std::unordered_map<std::string, std::string> & query_params) {
    query_params_ = query_params;
}

// path路径参数 /:app/:stream  app和stream就是路径参数
const std::string & HttpRequest::get_path_param(const std::string & k) const {
    static std::string empty_str("");
    auto it = path_params_.find(k);
    if (it == path_params_.end()) {
        return empty_str;
    }
    return it->second;
}

void HttpRequest::add_path_param(const std::string & k, const std::string & v) {
    path_params_[k] = v;
}

std::unordered_map<std::string, std::string> & HttpRequest::path_params() {
    return path_params_;
}

void HttpRequest::set_scheme(const std::string & scheme) {
    scheme_ = scheme;
}

const std::string & HttpRequest::get_scheme() const {
    return scheme_;
}

const std::string & HttpRequest::get_version() {
    return version_;
}

void HttpRequest::set_version(const std::string & v) {
    version_ = v;
}

const std::string & HttpRequest::get_version() const {
    return version_;
}

void HttpRequest::set_body(const std::string & body) {
    body_ = body;
}

const std::string & HttpRequest::get_body() const {
    return body_;
}

std::string HttpRequest::to_req_string() const {
    std::ostringstream oss;
    oss << method_str_ << " " << path_;
    if (query_params_.size() > 0) {
        oss << "?";
        size_t i = 0;
        for (auto & param : query_params_) {
            if (i == query_params_.size() - 1) {
                oss << param.first << "=" << param.second;
            } else {
                oss << param.first << "=" << param.second << "&";
            }
            i++;
        }
    }
    oss << " HTTP/" << version_ << HTTP_CRLF;
    if (headers_.size() > 0) {
        for (auto & p : headers_) {
            oss << p.first << ": " << p.second << HTTP_CRLF;
        }
    }
    oss << HTTP_CRLF;
    if (body_.size() > 0) {
        oss << body_;
    }
    return oss.str();
}

bool HttpRequest::parse_request_line(const char *buf, size_t len) {
    std::string s(buf, len);
    std::vector<std::string> vs;
    boost::split(vs, s, boost::is_any_of(" "));
    if (vs.size() != 3) {
        return false;
    }
    // get method
    const std::string & m = vs[0];
    if (m == "GET") {
        method_ = GET;
    } else if (m == "POST") {
        method_ = POST;
    } else if (m == "HEAD") {
        method_ = HEAD;
    } else if (m == "OPTION") {
        method_ = OPTION;
    } else if (m == "PUT") {
        method_ = PUT;
    } else if (m == "DELETE") {
        method_ = DELETE;
    } else {
        return false;
    }
    //get path
    const std::string & p = vs[1];
    auto pos = p.find("?");
    if (pos != std::string::npos) {
        path_ = p.substr(0, pos);
        std::string params = p.substr(pos+1);
        std::vector<std::string> vsp;
        boost::split(vsp, params, boost::is_any_of("&"));
        for(auto & s : vsp) {
            auto equ_pos = s.find("=");
            if (equ_pos == std::string::npos) {
                continue;
            }

            std::string name = s.substr(0, equ_pos);
            std::string value = s.substr(equ_pos + 1);
            query_params_[name] = value;
        }
    } else {
        path_ = p;
    }
    // HTTP/1.1
    const std::string & h = vs[2];
    std::vector<std::string> hv;
    boost::split(hv, h, boost::is_any_of("/"));
    if (hv.size() < 2) {
        return false;
    }

    if (hv[0] != "HTTP") {
        return false;
    }
    scheme_ = hv[0];
    version_ = hv[1];

    return true;
}

bool HttpRequest::parse_header(const char *buf, size_t len) {
    std::string_view s(buf, len);
    std::vector<std::string> vs;
    auto pos = s.find_first_of(":");
    if (pos == std::string_view::npos) {
        return false;
    }
    const std::string_view & attr = s.substr(0, pos);
    const std::string_view & val = s.substr(pos + 1);
    std::string sattr(attr.data(), attr.size());
    std::string sval(val.data(), val.size());
    boost::algorithm::trim(sval);
    headers_[sattr] = sval;
    return true;
}

bool HttpRequest::parse_body(const char *buf, size_t len) {
    body_.append(buf, len);
    return true;
}