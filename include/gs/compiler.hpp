#pragma once

#include "gs/bytecode.hpp"
#include "gs/parser.hpp"

#include <string>

namespace gs {

class Compiler {
public:
    Module compile(const Program& program);
};

Module compileSource(const std::string& source);
Module compileSourceFile(const std::string& path, const std::vector<std::string>& searchPaths = {});
std::string serializeModuleText(const Module& module);
Module deserializeModuleText(const std::string& text);
std::string generateAotCpp(const Module& module, const std::string& variableName);

} // namespace gs
