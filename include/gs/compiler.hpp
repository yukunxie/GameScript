#pragma once

#include "gs/bytecode.hpp"
#include "gs/ir.hpp"
#include "gs/parser.hpp"

#include <string>

namespace gs {

class Compiler {
public:
    Module compile(const Program& program);
    const std::vector<FunctionIR>& lastFunctionIR() const;

private:
    std::vector<FunctionIR> lastFunctionIR_;
};

Module compileSource(const std::string& source);
Module compileSourceFile(const std::string& path,
                         const std::vector<std::string>& searchPaths = {},
                         bool dumpTransformedSource = false);
void setCompileDisassemblyDumpEnabled(bool enabled);
bool compileDisassemblyDumpEnabled();
std::string serializeModuleText(const Module& module);
Module deserializeModuleText(const std::string& text);
std::string generateAotCpp(const Module& module, const std::string& variableName);

} // namespace gs
