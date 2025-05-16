#pragma once
#include "yaml-cpp/yaml.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

namespace mms {
class PlaceHolder;
class Param;
class PlaceHolderGenerator {
public:

    static int32_t parse_string_to_placeholders(const YAML::Node & node, 
                                                const std::string & unformatte_str, 
                                                std::vector<std::shared_ptr<PlaceHolder>> & holders);

    static int32_t parse_method_param(const YAML::Node &node, 
                                      std::shared_ptr<Param> param_holder, 
                                      const std::string & method_param, 
                                      std::map<std::string, std::shared_ptr<Param>> & params);

    static std::shared_ptr<PlaceHolder> gen_holder(const YAML::Node & node, 
                                                   std::string & seg, bool is_string_holder, 
                                                   std::map<std::string, std::shared_ptr<Param>> & params);

    static std::shared_ptr<Param> recursive_search_param_node(const YAML::Node & node, 
                                                              const std::string & param_name,
                                                              std::map<std::string, std::shared_ptr<Param>> & params);
};
};