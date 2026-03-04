#pragma once

#include "gs/export.hpp"
#include "gs/bytecode.hpp"
#include "gs/ir.hpp"
#include "gs/parser.hpp"

#include <string>

namespace gs {

class GS_API Compiler {
public:
    Module compile(const Program& program);
    const std::vector<FunctionIR>& lastFunctionIR() const;

private:
    std::vector<FunctionIR> lastFunctionIR_;
};

GS_API Module compileSource(const std::string& source);
GS_API Module compileSourceFile(const std::string& path,
                                const std::vector<std::string>& searchPaths = {},
                                bool dumpTransformedSource = false);
GS_API void setCompileDisassemblyDumpEnabled(bool enabled);
GS_API bool compileDisassemblyDumpEnabled();
GS_API std::string serializeModuleText(const Module& module);
GS_API Module deserializeModuleText(const std::string& text);
GS_API std::string generateAotCpp(const Module& module, const std::string& variableName);

} // namespace gs
