#include "extmap.hpp"
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "base/utils/utils.h"
using namespace mms;
std::string Extmap::prefix = "a=extmap:";
bool Extmap::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());
    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() < 2) {
        return false;
    }

    std::vector<std::string> vtmp;
    boost::split(vtmp, vs[0], boost::is_any_of("/"));
    value = vtmp[0];
    if (vtmp.size() == 2) {
        direction = vtmp[1];
    }
    uri = vs[1];
    
    return true;
}