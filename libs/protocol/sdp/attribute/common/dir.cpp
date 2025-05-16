#include <sstream>
#include <boost/algorithm/string.hpp>
#include "dir.hpp"
using namespace mms;
bool DirAttr::is_my_prefix(const std::string & line) {
    if (boost::starts_with(line, "a=recvonly") || boost::starts_with(line, "a=sendonly") || boost::starts_with(line, "a=sendrecv")) {
        return true;
    }
    return false;
}

bool DirAttr::parse(const std::string &line)
{
    if (boost::starts_with(line, "a=recvonly")) {
        dir_ = MEDIA_RECVONLY;
    } else if (boost::starts_with(line, "a=sendonly")) {
        dir_ = MEDIA_SENDONLY;
    } else if (boost::starts_with(line, "a=sendrecv")) {
        dir_ = MEDIA_SENDRECV;
    }
    return true;
}

std::string DirAttr::to_string() const
{
    std::ostringstream oss;
    if (dir_ == MEDIA_RECVONLY) {
        oss << "a=recvonly" << std::endl;
    } else if (dir_ == MEDIA_SENDONLY) {
        oss << "a=sendonly" << std::endl;
    } else if (dir_ == MEDIA_SENDRECV) {
        oss << "a=sendrecv" << std::endl;
    }
    
    return oss.str();
}