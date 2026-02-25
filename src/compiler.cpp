#include "gs/compiler.hpp"

#include "gs/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace gs {

namespace {

std::string trimCopy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string readFileText(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool defaultCompileDisassemblyDumpEnabled() {
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

bool g_compileDisassemblyDumpEnabled = defaultCompileDisassemblyDumpEnabled();

const char* opcodeName(OpCode op) {
    switch (op) {
    case OpCode::PushConst: return "PushConst";
    case OpCode::LoadLocal: return "LoadLocal";
    case OpCode::LoadName: return "LoadName";
    case OpCode::StoreName: return "StoreName";
    case OpCode::StoreLocal: return "StoreLocal";
    case OpCode::Add: return "Add";
    case OpCode::Sub: return "Sub";
    case OpCode::Mul: return "Mul";
    case OpCode::Div: return "Div";
    case OpCode::FloorDiv: return "FloorDiv";
    case OpCode::Mod: return "Mod";
    case OpCode::Pow: return "Pow";
    case OpCode::LessThan: return "LessThan";
    case OpCode::GreaterThan: return "GreaterThan";
    case OpCode::Equal: return "Equal";
    case OpCode::NotEqual: return "NotEqual";
    case OpCode::LessEqual: return "LessEqual";
    case OpCode::GreaterEqual: return "GreaterEqual";
    case OpCode::Is: return "Is";
    case OpCode::IsNot: return "IsNot";
    case OpCode::BitwiseAnd: return "BitwiseAnd";
    case OpCode::BitwiseOr: return "BitwiseOr";
    case OpCode::BitwiseXor: return "BitwiseXor";
    case OpCode::BitwiseNot: return "BitwiseNot";
    case OpCode::ShiftLeft: return "ShiftLeft";
    case OpCode::ShiftRight: return "ShiftRight";
    case OpCode::LogicalAnd: return "LogicalAnd";
    case OpCode::LogicalOr: return "LogicalOr";
    case OpCode::In: return "In";
    case OpCode::NotIn: return "NotIn";
    case OpCode::Negate: return "Negate";
    case OpCode::Not: return "Not";
    case OpCode::Jump: return "Jump";
    case OpCode::JumpIfFalse: return "JumpIfFalse";
    case OpCode::JumpIfFalseReg: return "JumpIfFalseReg";
    case OpCode::CallHost: return "CallHost";
    case OpCode::CallFunc: return "CallFunc";
    case OpCode::NewInstance: return "NewInstance";
    case OpCode::LoadAttr: return "LoadAttr";
    case OpCode::StoreAttr: return "StoreAttr";
    case OpCode::CallMethod: return "CallMethod";
    case OpCode::CallValue: return "CallValue";
    case OpCode::CallIntrinsic: return "CallIntrinsic";
    case OpCode::SpawnFunc: return "SpawnFunc";
    case OpCode::Await: return "Await";
    case OpCode::MakeList: return "MakeList";
    case OpCode::MakeDict: return "MakeDict";
    case OpCode::Sleep: return "Sleep";
    case OpCode::Yield: return "Yield";
    case OpCode::Return: return "Return";
    case OpCode::Pop: return "Pop";
    case OpCode::MoveLocalToReg: return "MoveLocalToReg";
    case OpCode::MoveNameToReg: return "MoveNameToReg";
    case OpCode::ConstToReg: return "ConstToReg";
    case OpCode::LoadConst: return "LoadConst";
    case OpCode::PushReg: return "PushReg";
    case OpCode::CaptureLocal: return "CaptureLocal";
    case OpCode::PushCapture: return "PushCapture";
    case OpCode::LoadCapture: return "LoadCapture";
    case OpCode::StoreCapture: return "StoreCapture";
    case OpCode::MakeClosure: return "MakeClosure";
    case OpCode::StoreLocalFromReg: return "StoreLocalFromReg";
    case OpCode::StoreNameFromReg: return "StoreNameFromReg";
    case OpCode::PushLocal: return "PushLocal";
    case OpCode::PushName: return "PushName";
    }
    return "Unknown";
}

std::string valueForDis(const Module& module, const Value& value) {
    switch (value.type) {
    case ValueType::Nil:
    case ValueType::Int:
    case ValueType::Float:
    case ValueType::Ref:
    case ValueType::Function:
    case ValueType::Class:
    case ValueType::Module: {
        std::ostringstream out;
        out << value;
        return out.str();
    }
    case ValueType::String: {
        const auto index = static_cast<std::size_t>(value.asStringIndex());
        if (index < module.strings.size()) {
            return std::string("\"") + module.strings[index] + "\"";
        }
        return std::string("str#") + std::to_string(index);
    }
    }
    return "?";
}

bool isTemporaryLocalName(const std::string& name) {
    return name.rfind("__", 0) == 0;
}

std::string formatLocalSlotForDis(std::int32_t slot, const FunctionIR* ir) {
    std::string text = std::string("local[") + std::to_string(slot) + "]";
    if (!ir || slot < 0) {
        return text;
    }
    const auto index = static_cast<std::size_t>(slot);
    if (index >= ir->localDebugNames.size()) {
        return text;
    }
    const std::string& localName = ir->localDebugNames[index];
    if (localName.empty() || !isTemporaryLocalName(localName)) {
        return text;
    }
    return text + "{tmp:" + localName + "}";
}

std::string formatSlotOperandForDis(const Module& module,
                                    SlotType slotType,
                                    std::int32_t slot,
                                    const FunctionIR* ir) {
    switch (slotType) {
    case SlotType::None:
        return "none";
    case SlotType::Local:
        return formatLocalSlotForDis(slot, ir);
    case SlotType::Constant: {
        const auto index = static_cast<std::size_t>(slot);
        if (index < module.constants.size()) {
            return std::string("const[") + std::to_string(slot) + "]=" + valueForDis(module, module.constants[index]);
        }
        return std::string("const[") + std::to_string(slot) + "]";
    }
    case SlotType::Register:
        return std::string("reg[") + std::to_string(slot) + "]";
    case SlotType::UpValue:
        return std::string("capture[") + std::to_string(slot) + "]";
    }
    return "?";
}

std::string bytecodeOperandHint(const Module& module, const Instruction& ins, const FunctionIR* ir = nullptr) {
    switch (ins.op) {
    case OpCode::PushConst: {
        const auto index = static_cast<std::size_t>(ins.a);
        if (index < module.constants.size()) {
            return std::string("const[") + std::to_string(ins.a) + "]=" + valueForDis(module, module.constants[index]);
        }
        return std::string("const[") + std::to_string(ins.a) + "]";
    }
    case OpCode::LoadName:
    case OpCode::PushName:
    case OpCode::StoreName:
    case OpCode::LoadAttr:
    case OpCode::StoreAttr:
    case OpCode::CallHost:
    case OpCode::CallMethod: {
        const auto index = static_cast<std::size_t>(ins.a);
        if (index < module.strings.size()) {
            return std::string("name[") + std::to_string(ins.a) + "]=" + module.strings[index];
        }
        return std::string("name[") + std::to_string(ins.a) + "]";
    }
    case OpCode::CallFunc:
    case OpCode::SpawnFunc: {
        const auto index = static_cast<std::size_t>(ins.a);
        if (index < module.functions.size()) {
            return std::string("fn[") + std::to_string(ins.a) + "]=" + module.functions[index].name;
        }
        return std::string("fn[") + std::to_string(ins.a) + "]";
    }
    case OpCode::Jump:
    case OpCode::JumpIfFalse:
    case OpCode::JumpIfFalseReg:
        return std::string("target=") + std::to_string(ins.a);
    case OpCode::LoadLocal:
    case OpCode::PushLocal:
    case OpCode::StoreLocal:
    case OpCode::MoveLocalToReg:
    case OpCode::CaptureLocal:
    case OpCode::StoreLocalFromReg:
        return formatLocalSlotForDis(ins.a, ir);
    case OpCode::PushCapture:
    case OpCode::LoadCapture:
    case OpCode::StoreCapture:
        return std::string("capture[") + std::to_string(ins.a) + "]";
    case OpCode::MakeClosure:
        return std::string("fn=") + std::to_string(ins.a) + " capture_count=" + std::to_string(ins.b);
    case OpCode::LoadConst: {
        const auto constIndex = static_cast<std::size_t>(ins.a);
        std::string valueHint = std::string("const[") + std::to_string(ins.a) + "]";
        if (constIndex < module.constants.size()) {
            valueHint += "=" + valueForDis(module, module.constants[constIndex]);
        }
        return valueHint + " -> " + formatLocalSlotForDis(ins.b, ir);
    }
    case OpCode::MoveNameToReg:
    case OpCode::StoreNameFromReg: {
        const auto index = static_cast<std::size_t>(ins.a);
        if (index < module.strings.size()) {
            return std::string("name[") + std::to_string(ins.a) + "]=" + module.strings[index];
        }
        return std::string("name[") + std::to_string(ins.a) + "]";
    }
    case OpCode::ConstToReg: {
        const auto index = static_cast<std::size_t>(ins.a);
        if (index < module.constants.size()) {
            return std::string("const[") + std::to_string(ins.a) + "]=" + valueForDis(module, module.constants[index]);
        }
        return std::string("const[") + std::to_string(ins.a) + "]";
    }
    case OpCode::Add:
    case OpCode::Sub:
    case OpCode::Mul:
    case OpCode::Div:
    case OpCode::FloorDiv:
    case OpCode::Mod:
    case OpCode::Pow:
    case OpCode::LessThan:
    case OpCode::GreaterThan:
    case OpCode::Equal:
    case OpCode::NotEqual:
    case OpCode::LessEqual:
    case OpCode::GreaterEqual:
    case OpCode::Is:
    case OpCode::IsNot:
    case OpCode::BitwiseAnd:
    case OpCode::BitwiseOr:
    case OpCode::BitwiseXor:
    case OpCode::ShiftLeft:
    case OpCode::ShiftRight:
    case OpCode::LogicalAnd:
    case OpCode::LogicalOr:
    case OpCode::In:
    case OpCode::NotIn:
        if (ins.aSlotType != SlotType::None || ins.bSlotType != SlotType::None) {
            return formatSlotOperandForDis(module, ins.aSlotType, ins.a, ir) + ", " +
                   formatSlotOperandForDis(module, ins.bSlotType, ins.b, ir) + " -> reg[0]";
        }
        return {};
    case OpCode::Negate:
    case OpCode::Not:
    case OpCode::BitwiseNot:
        if (ins.aSlotType != SlotType::None) {
            return formatSlotOperandForDis(module, ins.aSlotType, ins.a, ir) + " -> reg[0]";
        }
        return {};
    default:
        return {};
    }
}

std::string irOperandHint(const Module& module, const IRInstruction& ins) {
    const Instruction lowered{ins.op, ins.aSlotType, ins.a, ins.bSlotType, ins.b};
    return bytecodeOperandHint(module, lowered);
}

void writeTextStrict(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("error: failed to write compiler debug output: " + path.string() + " [function: <module>]");
    }
    out << text;
    out.flush();
    if (!out) {
        throw std::runtime_error("error: failed to flush compiler debug output: " + path.string() + " [function: <module>]");
    }
}

std::string buildIrDisassemblyText(const std::string& sourcePath,
                                   const Module& module,
                                   const std::vector<FunctionIR>& functionIrs) {
    std::ostringstream out;
    out << "# GameScript IR Disassembly\n";
    out << "source: " << sourcePath << "\n";
    out << "function_count: " << functionIrs.size() << "\n\n";
    for (const auto& ir : functionIrs) {
        out << "func " << ir.name << "(";
        for (std::size_t i = 0; i < ir.params.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << ir.params[i];
        }
        out << ") locals=" << ir.localCount << " est_stack=" << estimateStackSlots(ir) << "\n";
        out << " idx  line:col  op               a      b      delta  note\n";
        out << " ---- --------- ---------------- ------ ------ ------ ----------------\n";
        for (std::size_t i = 0; i < ir.code.size(); ++i) {
            const auto& ins = ir.code[i];
            out << std::setw(4) << i << " "
                << std::setw(4) << ins.line << ":" << std::left << std::setw(4) << ins.column << std::right << " "
                << std::left << std::setw(16) << opcodeName(ins.op) << std::right << " "
                << std::setw(6) << ins.a << " "
                << std::setw(6) << ins.b << " "
                << std::setw(6) << stackDelta(ins) << " "
                << irOperandHint(module, ins) << "\n";
        }
        out << "\n";
    }
    return out.str();
}

std::string buildBytecodeDisassemblyText(const std::string& sourcePath,
                                         const Module& module,
                                         const std::vector<FunctionIR>& functionIrs) {
    std::ostringstream out;
    out << "# GameScript Bytecode Disassembly\n";
    out << "source: " << sourcePath << "\n";
    out << "constants: " << module.constants.size() << "\n";
    for (std::size_t i = 0; i < module.constants.size(); ++i) {
        out << "  [" << i << "] " << valueForDis(module, module.constants[i]) << "\n";
    }
    out << "strings: " << module.strings.size() << "\n";
    for (std::size_t i = 0; i < module.strings.size(); ++i) {
        out << "  [" << i << "] " << module.strings[i] << "\n";
    }
    out << "\nfunctions: " << module.functions.size() << "\n\n";

    std::unordered_map<std::string, const FunctionIR*> irByName;
    for (const auto& ir : functionIrs) {
        irByName[ir.name] = &ir;
    }

    for (const auto& fn : module.functions) {
        const FunctionIR* ir = nullptr;
        auto foundIr = irByName.find(fn.name);
        if (foundIr != irByName.end()) {
            ir = foundIr->second;
        }
        out << "func " << fn.name << "(";
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << fn.params[i];
        }
        out << ") locals=" << fn.localCount << " stack_slots=" << fn.stackSlotCount << "\n";
        out << " idx  off  line:col  op               a      b      note\n";
        out << " ---- ---- --------- ---------------- ------ ------ ----------------\n";
        for (std::size_t i = 0; i < fn.code.size(); ++i) {
            const auto& ins = fn.code[i];
            std::size_t line = 0;
            std::size_t column = 0;
            if (ir && i < ir->code.size()) {
                line = ir->code[i].line;
                column = ir->code[i].column;
            }
            out << std::setw(4) << i << " "
                << std::setw(4) << i << " "
                << std::setw(4) << line << ":" << std::left << std::setw(4) << column << std::right << " "
                << std::left << std::setw(16) << opcodeName(ins.op) << std::right << " "
                << std::setw(6) << ins.a << " "
                << std::setw(6) << ins.b << " "
                << bytecodeOperandHint(module, ins, ir) << "\n";
        }
        out << "\n";
    }

    return out.str();
}

void dumpCompilerDebugFiles(const std::string& sourcePath,
                            const Module& module,
                            const std::vector<FunctionIR>& functionIrs) {
    namespace fs = std::filesystem;
    fs::path source(sourcePath);
    fs::path outputDir = source.parent_path() / ".gsdebug";
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        throw std::runtime_error("error: failed to create compiler debug directory: " + outputDir.string() + " [function: <module>]");
    }

    const std::string stem = source.stem().string();
    const fs::path irPath = outputDir / (stem + ".ir.dis");
    const fs::path opPath = outputDir / (stem + ".opcode.dis");
    writeTextStrict(irPath, buildIrDisassemblyText(sourcePath, module, functionIrs));
    writeTextStrict(opPath, buildBytecodeDisassemblyText(sourcePath, module, functionIrs));
}

std::vector<std::string> splitLines(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    if (!source.empty() && source.back() == '\n') {
        lines.push_back("");
    }
    return lines;
}

std::string normalizeModuleSpecToPath(const std::string& moduleSpec) {
    std::string candidate = moduleSpec;
    if (candidate.find('/') == std::string::npos && candidate.find('\\') == std::string::npos) {
        for (char& c : candidate) {
            if (c == '.') {
                c = '/';
            }
        }
    }
    return candidate;
}

std::string resolveImportPath(const std::string& moduleSpec,
                              const std::string& currentFile,
                              const std::vector<std::string>& searchPaths) {
    namespace fs = std::filesystem;

    std::vector<std::string> candidates;
    const std::string normalized = normalizeModuleSpecToPath(moduleSpec);
    candidates.push_back(normalized);
    if (normalized.size() < 3 || normalized.substr(normalized.size() - 3) != ".gs") {
        candidates.push_back(normalized + ".gs");
    }

    const fs::path currentDir = fs::path(currentFile).parent_path();
    for (const auto& candidate : candidates) {
        fs::path pathCandidate(candidate);
        if (pathCandidate.is_absolute() && fs::exists(pathCandidate)) {
            return fs::weakly_canonical(pathCandidate).string();
        }

        fs::path local = currentDir / pathCandidate;
        if (fs::exists(local)) {
            return fs::weakly_canonical(local).string();
        }

        for (const auto& base : searchPaths) {
            fs::path searchCandidate = fs::path(base) / pathCandidate;
            if (fs::exists(searchCandidate)) {
                return fs::weakly_canonical(searchCandidate).string();
            }
        }
    }

    return {};
}

std::string replaceAllRegex(const std::string& input,
                            const std::string& pattern,
                            const std::string& replacement) {
    return std::regex_replace(input, std::regex(pattern), replacement);
}

struct ImportStatement {
    std::string moduleSpec;
    std::vector<std::string> importNames;
    std::string alias;
    bool valid{false};
    bool isFrom{false};
    bool isWildcard{false};
};

struct ModuleAliasBinding {
    std::string alias;
    std::string moduleSpec;
    std::vector<std::string> exports;
};

struct ProcessedModule {
    std::string source;
    std::vector<std::string> exports;
};

std::string formatCompilerError(const std::string& message,
                                const std::string& functionName,
                                std::size_t line,
                                std::size_t column);

std::string defaultModuleAlias(const std::string& moduleSpec) {
    const std::size_t slash = moduleSpec.find_last_of("/\\");
    const std::size_t dot = moduleSpec.find_last_of('.');
    std::size_t split = std::string::npos;
    if (slash != std::string::npos && dot != std::string::npos) {
        split = std::max(slash, dot);
    } else if (slash != std::string::npos) {
        split = slash;
    } else if (dot != std::string::npos) {
        split = dot;
    }
    if (split == std::string::npos || split + 1 >= moduleSpec.size()) {
        return moduleSpec;
    }
    return moduleSpec.substr(split + 1);
}

void appendUnique(std::vector<std::string>& target, const std::string& value) {
    for (const auto& existing : target) {
        if (existing == value) {
            return;
        }
    }
    target.push_back(value);
}

std::vector<std::string> extractExportsFromSource(const std::string& source) {
    std::vector<std::string> exports;
    static const std::regex fnRe(R"(^\s*fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()");
    static const std::regex classRe(R"(^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)\b)");

    for (const auto& line : splitLines(source)) {
        std::smatch m;
        if (std::regex_search(line, m, fnRe)) {
            appendUnique(exports, m[1].str());
        }
        if (std::regex_search(line, m, classRe)) {
            appendUnique(exports, m[1].str());
        }
    }
    return exports;
}

std::string buildModuleAliasInitBlock(const ModuleAliasBinding& binding) {
    std::ostringstream out;
    out << "let " << binding.alias << " = loadModule(\"" << binding.moduleSpec << "\"); ";
    for (const auto& symbol : binding.exports) {
        out << binding.alias << "." << symbol << " = " << symbol << "; ";
    }
    return out.str();
}

std::string makeInternalAlias(const std::string& moduleSpec, std::size_t index) {
    std::string out = "__m" + std::to_string(index) + "_";
    for (char c : moduleSpec) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

std::string injectIntoFunctionBodies(const std::string& source, const std::string& injectionBlock) {
    if (injectionBlock.empty()) {
        return source;
    }

    static const std::regex fnHeaderRe(R"((^\s*fn\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^)]*\)\s*\{))");
    const auto injectLines = splitLines(injectionBlock);
    std::ostringstream out;
    for (const auto& line : splitLines(source)) {
        if (!std::regex_search(line, fnHeaderRe)) {
            out << line << '\n';
            continue;
        }

        out << line << '\n';
        std::size_t indentWidth = 0;
        while (indentWidth < line.size() && std::isspace(static_cast<unsigned char>(line[indentWidth]))) {
            ++indentWidth;
        }
        const std::string indent = line.substr(0, indentWidth) + "    ";
        for (const auto& injectLine : injectLines) {
            if (injectLine.empty()) {
                continue;
            }
            out << indent << injectLine << '\n';
        }
    }
    return out.str();
}

ImportStatement parseImportLine(const std::string& rawLine, std::size_t lineNo = 0) {
    ImportStatement stmt;
    std::string line = trimCopy(rawLine);
    if (line.empty()) {
        return stmt;
    }
    if (line.rfind("#", 0) == 0 || line.rfind("//", 0) == 0) {
        return stmt;
    }
    if (!line.empty() && line.back() == ';') {
        line.pop_back();
        line = trimCopy(line);
    }

    std::smatch match;
    static const std::regex importRe(R"(^import\s+([A-Za-z_][A-Za-z0-9_./]*)\s*(?:as\s+([A-Za-z_][A-Za-z0-9_]*))?$)");

    if (std::regex_match(line, match, importRe)) {
        stmt.valid = true;
        stmt.isFrom = false;
        stmt.moduleSpec = match[1].str();
        if (match.size() > 2) {
            stmt.alias = match[2].str();
        }
        stmt.isWildcard = true;
        return stmt;
    }

    if (line.rfind("from ", 0) == 0) {
        const std::size_t importPos = line.find(" import ");
        if (importPos == std::string::npos) {
            return stmt;
        }

        const std::string moduleSpec = trimCopy(line.substr(5, importPos - 5));
        if (moduleSpec.empty()) {
            return stmt;
        }

        std::string importSpec = trimCopy(line.substr(importPos + 8));
        std::string alias;
        static const std::regex identRe(R"(^[A-Za-z_][A-Za-z0-9_]*$)");

        const std::size_t asPos = importSpec.rfind(" as ");
        if (asPos != std::string::npos) {
            const std::string aliasCandidate = trimCopy(importSpec.substr(asPos + 4));
            if (aliasCandidate.empty() || !std::regex_match(aliasCandidate, identRe)) {
                return stmt;
            }
            alias = aliasCandidate;
            importSpec = trimCopy(importSpec.substr(0, asPos));
        }

        stmt.valid = true;
        stmt.isFrom = true;
        stmt.moduleSpec = moduleSpec;
        stmt.alias = alias;

        if (importSpec == "*") {
            stmt.isWildcard = true;
            return stmt;
        }

        std::istringstream namesIn(importSpec);
        std::string segment;
        while (std::getline(namesIn, segment, ',')) {
            std::string name = trimCopy(segment);
            if (name.empty() || !std::regex_match(name, identRe)) {
                throw std::runtime_error(formatCompilerError("Invalid import symbol in line: " + rawLine,
                                                             "<module>",
                                                             lineNo,
                                                             1));
            }
            stmt.importNames.push_back(name);
        }

        if (stmt.importNames.empty()) {
            throw std::runtime_error(formatCompilerError("from-import requires at least one symbol",
                                                         "<module>",
                                                         lineNo,
                                                         1));
        }
        return stmt;
    }

    return stmt;
}

ProcessedModule preprocessImportsRecursive(const std::string& filePath,
                                           const std::vector<std::string>& searchPaths,
                                           std::unordered_map<std::string, ProcessedModule>& cache,
                                           std::unordered_set<std::string>& visiting) {
    (void)searchPaths;
    const std::string canonical = std::filesystem::weakly_canonical(filePath).string();
    auto cached = cache.find(canonical);
    if (cached != cache.end()) {
        return cached->second;
    }
    if (visiting.contains(canonical)) {
        throw std::runtime_error(formatCompilerError("Cyclic import detected: " + canonical,
                                                     "<module>",
                                                     1,
                                                     1));
    }

    const std::string source = readFileText(canonical);
    if (source.empty()) {
        throw std::runtime_error(formatCompilerError("Failed to read script file: " + canonical,
                                                     "<module>",
                                                     1,
                                                     1));
    }

    visiting.insert(canonical);

    std::ostringstream bodySource;
    std::size_t importTempIndex = 0;

    const auto lines = splitLines(source);
    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const auto& line = lines[lineIndex];
        const std::size_t lineNo = lineIndex + 1;
        ImportStatement stmt = parseImportLine(line, lineNo);
        if (!stmt.valid) {
            bodySource << line << '\n';
            continue;
        }

        if (!stmt.isFrom) {
            const std::string alias = stmt.alias.empty() ? defaultModuleAlias(stmt.moduleSpec) : stmt.alias;
            bodySource << "let " << alias << " = loadModule(\"" << stmt.moduleSpec << "\");\n";
            continue;
        }

        if (stmt.isWildcard) {
            if (stmt.alias.empty()) {
                throw std::runtime_error(formatCompilerError("from " + stmt.moduleSpec + " import * requires alias in strict module mode",
                                                             "<module>",
                                                             lineNo,
                                                             1));
            }
            bodySource << "let " << stmt.alias << " = loadModule(\"" << stmt.moduleSpec << "\");\n";
            continue;
        }

        if (stmt.importNames.size() > 1) {
            if (stmt.alias.empty()) {
                throw std::runtime_error(formatCompilerError("from " + stmt.moduleSpec + " import a,b requires alias in strict module mode",
                                                             "<module>",
                                                             lineNo,
                                                             1));
            }

            bodySource << "let " << stmt.alias << " = loadModule(\"" << stmt.moduleSpec << "\"";
            for (const auto& symbol : stmt.importNames) {
                bodySource << ", \"" << symbol << "\"";
            }
            bodySource << ");\n";
            continue;
        }

        const std::string importedName = stmt.importNames.front();
        const std::string localName = stmt.alias.empty() ? importedName : stmt.alias;
        static const std::regex identRe(R"(^[A-Za-z_][A-Za-z0-9_]*$)");
        if (!std::regex_match(localName, identRe)) {
            throw std::runtime_error(formatCompilerError("Invalid local alias generated for from-import: " + localName,
                                                         "<module>",
                                                         lineNo,
                                                         1));
        }
                                bodySource << "let " << localName << " = loadModule(\"" << stmt.moduleSpec << "\", \"" << importedName << "\");\n";
    }

    visiting.erase(canonical);

    ProcessedModule processed;
    processed.source = bodySource.str();
    processed.exports = extractExportsFromSource(processed.source);
    cache.emplace(canonical, processed);
    return processed;
}

struct LoopContext {
    std::vector<std::size_t> breakJumps;
    std::vector<std::size_t> continueJumps;
    std::size_t continueTarget{0};
};

thread_local std::unordered_map<std::string, std::size_t>* g_mutableFuncIndex = nullptr;
thread_local std::size_t* g_lambdaOrdinal = nullptr;
thread_local std::vector<FunctionIR>* g_allFunctionIrs = nullptr;

void collectCapturedNamesInExpr(const Expr& expr,
                                const std::unordered_map<std::string, std::size_t>& outerLocals,
                                const std::unordered_set<std::string>& lambdaLocals,
                                std::vector<std::string>& outCaptureNames,
                                std::unordered_set<std::string>& dedup) {
    auto maybeCapture = [&](const std::string& name) {
        if (lambdaLocals.contains(name)) {
            return;
        }
        if (!outerLocals.contains(name)) {
            return;
        }
        if (dedup.insert(name).second) {
            outCaptureNames.push_back(name);
        }
    };

    switch (expr.type) {
    case ExprType::Variable:
        maybeCapture(expr.name);
        return;
    case ExprType::AssignVariable:
        maybeCapture(expr.name);
        if (expr.right) {
            collectCapturedNamesInExpr(*expr.right, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::AssignProperty:
        if (expr.object) {
            collectCapturedNamesInExpr(*expr.object, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        if (expr.right) {
            collectCapturedNamesInExpr(*expr.right, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::AssignIndex:
        if (expr.object) {
            collectCapturedNamesInExpr(*expr.object, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        if (expr.index) {
            collectCapturedNamesInExpr(*expr.index, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        if (expr.right) {
            collectCapturedNamesInExpr(*expr.right, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::Unary:
        if (expr.right) {
            collectCapturedNamesInExpr(*expr.right, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::Binary:
        if (expr.left) {
            collectCapturedNamesInExpr(*expr.left, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        if (expr.right) {
            collectCapturedNamesInExpr(*expr.right, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::Call:
        if (expr.callee) {
            collectCapturedNamesInExpr(*expr.callee, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        for (const auto& arg : expr.args) {
            collectCapturedNamesInExpr(arg, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::MethodCall:
        if (expr.object) {
            collectCapturedNamesInExpr(*expr.object, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        for (const auto& arg : expr.args) {
            collectCapturedNamesInExpr(arg, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::PropertyAccess:
        if (expr.object) {
            collectCapturedNamesInExpr(*expr.object, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::IndexAccess:
        if (expr.object) {
            collectCapturedNamesInExpr(*expr.object, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        if (expr.index) {
            collectCapturedNamesInExpr(*expr.index, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::ListLiteral:
        for (const auto& element : expr.listElements) {
            collectCapturedNamesInExpr(element, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::DictLiteral:
        for (const auto& entry : expr.dictEntries) {
            collectCapturedNamesInExpr(*entry.key, outerLocals, lambdaLocals, outCaptureNames, dedup);
            collectCapturedNamesInExpr(*entry.value, outerLocals, lambdaLocals, outCaptureNames, dedup);
        }
        return;
    case ExprType::Lambda:
    case ExprType::Number:
    case ExprType::StringLiteral:
        return;
    }
}

void collectCapturedNamesInStatements(const std::vector<Stmt>& statements,
                                      const std::unordered_map<std::string, std::size_t>& outerLocals,
                                      const std::unordered_set<std::string>& lambdaLocals,
                                      std::vector<std::string>& outCaptureNames,
                                      std::unordered_set<std::string>& dedup) {
    for (const auto& stmt : statements) {
        switch (stmt.type) {
        case StmtType::LetExpr:
        case StmtType::Expr:
        case StmtType::Return:
            collectCapturedNamesInExpr(stmt.expr, outerLocals, lambdaLocals, outCaptureNames, dedup);
            break;
        case StmtType::ForRange:
            collectCapturedNamesInExpr(stmt.rangeStart, outerLocals, lambdaLocals, outCaptureNames, dedup);
            collectCapturedNamesInExpr(stmt.rangeEnd, outerLocals, lambdaLocals, outCaptureNames, dedup);
            collectCapturedNamesInStatements(stmt.body, outerLocals, lambdaLocals, outCaptureNames, dedup);
            break;
        case StmtType::ForList:
        case StmtType::ForDict:
            collectCapturedNamesInExpr(stmt.iterable, outerLocals, lambdaLocals, outCaptureNames, dedup);
            collectCapturedNamesInStatements(stmt.body, outerLocals, lambdaLocals, outCaptureNames, dedup);
            break;
        case StmtType::If:
            for (const auto& cond : stmt.branchConditions) {
                collectCapturedNamesInExpr(cond, outerLocals, lambdaLocals, outCaptureNames, dedup);
            }
            for (const auto& body : stmt.branchBodies) {
                collectCapturedNamesInStatements(body, outerLocals, lambdaLocals, outCaptureNames, dedup);
            }
            collectCapturedNamesInStatements(stmt.elseBody, outerLocals, lambdaLocals, outCaptureNames, dedup);
            break;
        case StmtType::While:
            collectCapturedNamesInExpr(stmt.condition, outerLocals, lambdaLocals, outCaptureNames, dedup);
            collectCapturedNamesInStatements(stmt.body, outerLocals, lambdaLocals, outCaptureNames, dedup);
            break;
        case StmtType::Break:
        case StmtType::Continue:
        case StmtType::LetSpawn:
        case StmtType::LetAwait:
        case StmtType::Sleep:
        case StmtType::Yield:
            break;
        }
    }
}

std::string formatCompilerError(const std::string& message,
                                const std::string& functionName,
                                std::size_t line,
                                std::size_t column) {
    std::ostringstream out;
    if (line > 0 && column > 0) {
        out << line << ":" << column << ": error: " << message;
    } else {
        out << "error: " << message;
    }
    out << " [function: " << (functionName.empty() ? "<module>" : functionName) << "]";
    return out.str();
}

std::string mangleMethodName(const std::string& className, const std::string& methodName) {
    return className + "::" + methodName;
}

std::int32_t addString(Module& module, const std::string& value);

Value evalClassFieldInit(const Expr& expr,
                        Module& module,
                        const std::unordered_map<std::string, std::size_t>& funcIndex,
                        const std::unordered_map<std::string, std::size_t>& classIndex,
                        const std::string& scopeName);

bool resolveNamedValue(const Module& module,
                       const std::unordered_map<std::string, std::size_t>& funcIndex,
                       const std::unordered_map<std::string, std::size_t>& classIndex,
                       const std::string& name,
                       Value& outValue) {
    for (const auto& global : module.globals) {
        if (global.name == name) {
            outValue = global.initialValue;
            return true;
        }
    }

    auto functionIt = funcIndex.find(name);
    if (functionIt != funcIndex.end()) {
        outValue = Value::Function(static_cast<std::int64_t>(functionIt->second));
        return true;
    }

    auto classIt = classIndex.find(name);
    if (classIt != classIndex.end()) {
        outValue = Value::Class(static_cast<std::int64_t>(classIt->second));
        return true;
    }

    return false;
}

struct SymbolLookupResult {
    bool found{false};
    std::size_t localSlot{0};
};

SymbolLookupResult resolveSymbol(const std::unordered_map<std::string, std::size_t>& locals,
                                 const Module& module,
                                 const std::unordered_map<std::string, std::size_t>& funcIndex,
                                 const std::unordered_map<std::string, std::size_t>& classIndex,
                                 const std::string& name) {
    SymbolLookupResult result;

    auto localIt = locals.find(name);
    if (localIt != locals.end()) {
        result.found = true;
        result.localSlot = localIt->second;
    }

    return result;
}

void validateLocalUsageInExpr(const Expr& expr,
                              const std::unordered_set<std::string>& localNames,
                              const std::unordered_set<std::string>& declaredNames,
                              const std::string& scopeName);

void validateLocalUsageInStatements(const std::vector<Stmt>& statements,
                                    const std::unordered_set<std::string>& localNames,
                                    std::unordered_set<std::string>& declaredNames,
                                    const std::string& scopeName) {
    for (const auto& stmt : statements) {
        switch (stmt.type) {
        case StmtType::LetExpr:
            declaredNames.insert(stmt.name);
            validateLocalUsageInExpr(stmt.expr, localNames, declaredNames, scopeName);
            break;
        case StmtType::LetSpawn:
            declaredNames.insert(stmt.name);
            break;
        case StmtType::LetAwait:
            declaredNames.insert(stmt.name);
            if (localNames.contains(stmt.awaitSource) && !declaredNames.contains(stmt.awaitSource)) {
                throw std::runtime_error(formatCompilerError("Local variable used before declaration: " + stmt.awaitSource,
                                                             scopeName,
                                                             stmt.line,
                                                             stmt.column));
            }
            break;
        case StmtType::ForRange:
            validateLocalUsageInExpr(stmt.rangeStart, localNames, declaredNames, scopeName);
            validateLocalUsageInExpr(stmt.rangeEnd, localNames, declaredNames, scopeName);
            declaredNames.insert(stmt.iterKey);
            validateLocalUsageInStatements(stmt.body, localNames, declaredNames, scopeName);
            break;
        case StmtType::ForList:
            validateLocalUsageInExpr(stmt.iterable, localNames, declaredNames, scopeName);
            declaredNames.insert(stmt.iterKey);
            validateLocalUsageInStatements(stmt.body, localNames, declaredNames, scopeName);
            break;
        case StmtType::ForDict:
            validateLocalUsageInExpr(stmt.iterable, localNames, declaredNames, scopeName);
            declaredNames.insert(stmt.iterKey);
            declaredNames.insert(stmt.iterValue);
            validateLocalUsageInStatements(stmt.body, localNames, declaredNames, scopeName);
            break;
        case StmtType::If:
            for (const auto& condition : stmt.branchConditions) {
                validateLocalUsageInExpr(condition, localNames, declaredNames, scopeName);
            }
            for (const auto& body : stmt.branchBodies) {
                validateLocalUsageInStatements(body, localNames, declaredNames, scopeName);
            }
            validateLocalUsageInStatements(stmt.elseBody, localNames, declaredNames, scopeName);
            break;
        case StmtType::While:
            validateLocalUsageInExpr(stmt.condition, localNames, declaredNames, scopeName);
            validateLocalUsageInStatements(stmt.body, localNames, declaredNames, scopeName);
            break;
        case StmtType::Expr:
        case StmtType::Return:
            validateLocalUsageInExpr(stmt.expr, localNames, declaredNames, scopeName);
            break;
        case StmtType::Break:
        case StmtType::Continue:
        case StmtType::Sleep:
        case StmtType::Yield:
            break;
        }
    }
}

void validateLocalUsageInExpr(const Expr& expr,
                              const std::unordered_set<std::string>& localNames,
                              const std::unordered_set<std::string>& declaredNames,
                              const std::string& scopeName) {
    switch (expr.type) {
    case ExprType::Variable:
        if (localNames.contains(expr.name) && !declaredNames.contains(expr.name)) {
            throw std::runtime_error(formatCompilerError("Local variable used before declaration: " + expr.name,
                                                         scopeName,
                                                         expr.line,
                                                         expr.column));
        }
        return;
    case ExprType::AssignVariable:
        if (localNames.contains(expr.name) && !declaredNames.contains(expr.name)) {
            throw std::runtime_error(formatCompilerError("Local variable used before declaration: " + expr.name,
                                                         scopeName,
                                                         expr.line,
                                                         expr.column));
        }
        if (expr.right) {
            validateLocalUsageInExpr(*expr.right, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::AssignProperty:
        if (expr.object) {
            validateLocalUsageInExpr(*expr.object, localNames, declaredNames, scopeName);
        }
        if (expr.right) {
            validateLocalUsageInExpr(*expr.right, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::AssignIndex:
        if (expr.object) {
            validateLocalUsageInExpr(*expr.object, localNames, declaredNames, scopeName);
        }
        if (expr.index) {
            validateLocalUsageInExpr(*expr.index, localNames, declaredNames, scopeName);
        }
        if (expr.right) {
            validateLocalUsageInExpr(*expr.right, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::Binary:
        if (expr.left) {
            validateLocalUsageInExpr(*expr.left, localNames, declaredNames, scopeName);
        }
        if (expr.right) {
            validateLocalUsageInExpr(*expr.right, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::Call:
        if (expr.callee) {
            validateLocalUsageInExpr(*expr.callee, localNames, declaredNames, scopeName);
        }
        for (const auto& arg : expr.args) {
            validateLocalUsageInExpr(arg, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::MethodCall:
        if (expr.object) {
            validateLocalUsageInExpr(*expr.object, localNames, declaredNames, scopeName);
        }
        for (const auto& arg : expr.args) {
            validateLocalUsageInExpr(arg, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::PropertyAccess:
        if (expr.object) {
            validateLocalUsageInExpr(*expr.object, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::IndexAccess:
        if (expr.object) {
            validateLocalUsageInExpr(*expr.object, localNames, declaredNames, scopeName);
        }
        if (expr.index) {
            validateLocalUsageInExpr(*expr.index, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::ListLiteral:
        for (const auto& element : expr.listElements) {
            validateLocalUsageInExpr(element, localNames, declaredNames, scopeName);
        }
        return;
    case ExprType::DictLiteral:
        for (const auto& entry : expr.dictEntries) {
            if (entry.key) {
                validateLocalUsageInExpr(*entry.key, localNames, declaredNames, scopeName);
            }
            if (entry.value) {
                validateLocalUsageInExpr(*entry.value, localNames, declaredNames, scopeName);
            }
        }
        return;
    case ExprType::Lambda:
        return;
    case ExprType::Number:
    case ExprType::StringLiteral:
        return;
    }
}

void collectLocalDeclarations(const std::vector<Stmt>& statements,
                              std::unordered_set<std::string>& localNames,
                              const std::string& scopeName) {
    for (const auto& stmt : statements) {
        switch (stmt.type) {
        case StmtType::LetExpr:
        case StmtType::LetSpawn:
        case StmtType::LetAwait:
            if (localNames.contains(stmt.name)) {
                throw std::runtime_error(formatCompilerError("Duplicate let declaration in scope: " + stmt.name,
                                                             scopeName,
                                                             stmt.line,
                                                             stmt.column));
            }
            localNames.insert(stmt.name);
            break;
        default:
            break;
        }

        if (!stmt.body.empty()) {
            collectLocalDeclarations(stmt.body, localNames, scopeName);
        }
        if (!stmt.elseBody.empty()) {
            collectLocalDeclarations(stmt.elseBody, localNames, scopeName);
        }
        for (const auto& branchBody : stmt.branchBodies) {
            if (!branchBody.empty()) {
                collectLocalDeclarations(branchBody, localNames, scopeName);
            }
        }
    }
}

void validateScopeLocalRules(const std::vector<Stmt>& statements,
                             const std::vector<std::string>& predeclaredNames,
                             const std::string& scopeName) {
    std::unordered_set<std::string> localNames;
    std::unordered_set<std::string> declaredNames;

    for (const auto& name : predeclaredNames) {
        if (localNames.contains(name)) {
            throw std::runtime_error(formatCompilerError("Duplicate parameter in scope: " + name,
                                                         scopeName,
                                                         0,
                                                         0));
        }
        localNames.insert(name);
        declaredNames.insert(name);
    }

    collectLocalDeclarations(statements, localNames, scopeName);
    validateLocalUsageInStatements(statements, localNames, declaredNames, scopeName);
}

Value evalClassFieldInit(const Expr& expr,
                        Module& module,
                        const std::unordered_map<std::string, std::size_t>& funcIndex,
                        const std::unordered_map<std::string, std::size_t>& classIndex,
                        const std::string& scopeName) {
    switch (expr.type) {
    case ExprType::Number:
        return expr.value;
    case ExprType::StringLiteral:
        return Value::String(addString(module, expr.stringLiteral));
    case ExprType::Variable: {
        Value namedValue = Value::Nil();
        if (resolveNamedValue(module, funcIndex, classIndex, expr.name, namedValue)) {
            return namedValue;
        }
        break;
    }
    default:
        break;
    }

    throw std::runtime_error(formatCompilerError("Class field initializer must be number/string/symbol name",
                                                 scopeName,
                                                 expr.line,
                                                 expr.column));
}

Value evalGlobalInit(const Expr& expr,
                    Module& module,
                    const std::unordered_map<std::string, std::size_t>& funcIndex,
                    const std::unordered_map<std::string, std::size_t>& classIndex,
                    const std::string& scopeName) {
    switch (expr.type) {
    case ExprType::Number:
        return expr.value;
    case ExprType::StringLiteral:
        return Value::String(addString(module, expr.stringLiteral));
    case ExprType::Variable: {
        Value namedValue = Value::Nil();
        if (resolveNamedValue(module, funcIndex, classIndex, expr.name, namedValue)) {
            return namedValue;
        }
        break;
    }
    default:
        break;
    }

    throw std::runtime_error(formatCompilerError("Top-level let initializer must be number/string/symbol name",
                                                 scopeName,
                                                 expr.line,
                                                 expr.column));
}

std::int32_t addConstant(Module& module, Value value) {
    module.constants.push_back(value);
    return static_cast<std::int32_t>(module.constants.size() - 1);
}

std::int32_t addString(Module& module, const std::string& value) {
    for (std::size_t i = 0; i < module.strings.size(); ++i) {
        if (module.strings[i] == value) {
            return static_cast<std::int32_t>(i);
        }
    }
    module.strings.push_back(value);
    return static_cast<std::int32_t>(module.strings.size() - 1);
}

void emit(std::vector<IRInstruction>& code,
          OpCode op,
          std::int32_t a = 0,
          std::int32_t b = 0,
          std::size_t line = 0,
          std::size_t column = 0,
          SlotType aSlotType = SlotType::None,
          SlotType bSlotType = SlotType::None) {
    code.push_back({op, aSlotType, a, bSlotType, b, line, column});
}

std::size_t emitJump(std::vector<IRInstruction>& code,
                     OpCode op,
                     std::size_t line = 0,
                     std::size_t column = 0) {
    code.push_back({op, SlotType::None, -1, SlotType::None, 0, line, column});
    return code.size() - 1;
}

std::size_t emitJumpIfFalseReg(std::vector<IRInstruction>& code,
                               std::size_t line = 0,
                               std::size_t column = 0) {
    code.push_back({OpCode::JumpIfFalseReg, SlotType::None, -1, SlotType::None, 0, line, column});
    return code.size() - 1;
}

void patchJump(std::vector<IRInstruction>& code, std::size_t jumpIndex, std::size_t targetIndex) {
    code[jumpIndex].a = static_cast<std::int32_t>(targetIndex);
}

std::size_t ensureLocal(std::unordered_map<std::string, std::size_t>& locals,
                        std::size_t& localCount,
                        const std::string& name,
                        FunctionIR* functionIr = nullptr) {
    auto it = locals.find(name);
    if (it != locals.end()) {
        return it->second;
    }
    const std::size_t slot = localCount;
    locals[name] = slot;
    if (functionIr) {
        if (functionIr->localDebugNames.size() <= slot) {
            functionIr->localDebugNames.resize(slot + 1);
        }
        functionIr->localDebugNames[slot] = name;
    }
    ++localCount;
    return slot;
}

void emitLocalValueToStack(std::vector<IRInstruction>& code, std::size_t slot) {
    emit(code, OpCode::PushLocal, static_cast<std::int32_t>(slot), 0);
}

void emitNameValueToStack(std::vector<IRInstruction>& code, std::int32_t nameIndex) {
    emit(code, OpCode::PushName, nameIndex, 0);
}

bool tryCompileExprToRegister(const Expr& expr,
                              Module& module,
                              const std::unordered_map<std::string, std::size_t>& locals,
                              const std::unordered_map<std::string, std::size_t>& funcIndex,
                              const std::unordered_map<std::string, std::size_t>& classIndex,
                              const std::string& currentFunctionName,
                              std::vector<IRInstruction>& code,
                              const std::unordered_map<std::string, std::size_t>* captureIndexByName = nullptr) {
    (void)currentFunctionName;
    auto resolveVariableSlot = [&](const std::string& name,
                                   SlotType& outSlotType,
                                   std::int32_t& outSlot) -> bool {
        if (captureIndexByName) {
            auto captureIt = captureIndexByName->find(name);
            if (captureIt != captureIndexByName->end()) {
                outSlotType = SlotType::UpValue;
                outSlot = static_cast<std::int32_t>(captureIt->second);
                return true;
            }
        }

        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          name);
        if (resolved.found) {
            outSlotType = SlotType::Local;
            outSlot = static_cast<std::int32_t>(resolved.localSlot);
            return true;
        }
        return false;
    };

    switch (expr.type) {
    case ExprType::Number:
        emit(code, OpCode::ConstToReg, addConstant(module, expr.value), 0);
        return true;
    case ExprType::StringLiteral:
        emit(code,
             OpCode::ConstToReg,
             addConstant(module, Value::String(addString(module, expr.stringLiteral))),
             0);
        return true;
    case ExprType::Variable: {
        if (captureIndexByName && captureIndexByName->contains(expr.name)) {
            return false;
        }
        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          expr.name);
        if (resolved.found) {
            emit(code, OpCode::MoveLocalToReg, static_cast<std::int32_t>(resolved.localSlot), 0);
        } else {
            emit(code, OpCode::MoveNameToReg, addString(module, expr.name), 0);
        }
        return true;
    }
    case ExprType::Unary: {
        if (!expr.right) {
            return false;
        }
        if (expr.right->type != ExprType::Variable) {
            return false;
        }

        SlotType operandSlotType = SlotType::None;
        std::int32_t operandSlot = -1;
        if (!resolveVariableSlot(expr.right->name, operandSlotType, operandSlot)) {
            return false;
        }

        switch (expr.unaryOp) {
        case TokenType::Minus:
            emit(code,
                 OpCode::Negate,
                 operandSlot,
                 0,
                 0,
                 0,
                 operandSlotType,
                 SlotType::None);
            return true;
        case TokenType::Bang:
            emit(code,
                 OpCode::Not,
                 operandSlot,
                 0,
                 0,
                 0,
                 operandSlotType,
                 SlotType::None);
            return true;
        default:
            return false;
        }
    }
    case ExprType::Binary: {
        if (!expr.left || !expr.right ||
            expr.left->type != ExprType::Variable ||
            expr.right->type != ExprType::Variable) {
            return false;
        }

        SlotType leftSlotType = SlotType::None;
        SlotType rightSlotType = SlotType::None;
        std::int32_t leftSlot = -1;
        std::int32_t rightSlot = -1;
        if (!resolveVariableSlot(expr.left->name, leftSlotType, leftSlot) ||
            !resolveVariableSlot(expr.right->name, rightSlotType, rightSlot)) {
            return false;
        }

        switch (expr.binaryOp) {
        case TokenType::Plus:
            emit(code, OpCode::Add, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Minus:
            emit(code, OpCode::Sub, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Star:
            emit(code, OpCode::Mul, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Slash:
            emit(code, OpCode::Div, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::SlashSlash:
            emit(code, OpCode::FloorDiv, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Percent:
            emit(code, OpCode::Mod, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::StarStar:
            emit(code, OpCode::Pow, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Less:
            emit(code, OpCode::LessThan, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Greater:
            emit(code, OpCode::GreaterThan, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::EqualEqual:
            emit(code, OpCode::Equal, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::BangEqual:
            // Check if this is actually "is not" (unaryOp == KeywordNot is the marker)
            if (expr.unaryOp == TokenType::KeywordNot) {
                emit(code, OpCode::IsNot, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            } else {
                emit(code, OpCode::NotEqual, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            }
            return true;
        case TokenType::LessEqual:
            emit(code, OpCode::LessEqual, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::GreaterEqual:
            emit(code, OpCode::GreaterEqual, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::KeywordIs:
            emit(code, OpCode::Is, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Amp:
            emit(code, OpCode::BitwiseAnd, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Pipe:
            emit(code, OpCode::BitwiseOr, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::Caret:
            emit(code, OpCode::BitwiseXor, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::ShiftLeft:
            emit(code, OpCode::ShiftLeft, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::ShiftRight:
            emit(code, OpCode::ShiftRight, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::AmpAmp:
            emit(code, OpCode::LogicalAnd, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::PipePipe:
            emit(code, OpCode::LogicalOr, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            return true;
        case TokenType::KeywordIn:
            // Check if this is "not in"
            if (expr.unaryOp == TokenType::KeywordNot) {
                emit(code, OpCode::NotIn, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            } else {
                emit(code, OpCode::In, leftSlot, rightSlot, 0, 0, leftSlotType, rightSlotType);
            }
            return true;
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

bool tryGetBinaryOp(TokenType token, OpCode& outOp) {
    switch (token) {
    case TokenType::Plus: outOp = OpCode::Add; return true;
    case TokenType::Minus: outOp = OpCode::Sub; return true;
    case TokenType::Star: outOp = OpCode::Mul; return true;
    case TokenType::Slash: outOp = OpCode::Div; return true;
    case TokenType::SlashSlash: outOp = OpCode::FloorDiv; return true;
    case TokenType::Percent: outOp = OpCode::Mod; return true;
    case TokenType::StarStar: outOp = OpCode::Pow; return true;
    case TokenType::Less: outOp = OpCode::LessThan; return true;
    case TokenType::Greater: outOp = OpCode::GreaterThan; return true;
    case TokenType::EqualEqual: outOp = OpCode::Equal; return true;
    case TokenType::BangEqual: outOp = OpCode::NotEqual; return true;
    case TokenType::LessEqual: outOp = OpCode::LessEqual; return true;
    case TokenType::GreaterEqual: outOp = OpCode::GreaterEqual; return true;
    case TokenType::KeywordIs: outOp = OpCode::Is; return true;
    case TokenType::Amp: outOp = OpCode::BitwiseAnd; return true;
    case TokenType::Pipe: outOp = OpCode::BitwiseOr; return true;
    case TokenType::Caret: outOp = OpCode::BitwiseXor; return true;
    case TokenType::ShiftLeft: outOp = OpCode::ShiftLeft; return true;
    case TokenType::ShiftRight: outOp = OpCode::ShiftRight; return true;
    case TokenType::AmpAmp: outOp = OpCode::LogicalAnd; return true;
    case TokenType::PipePipe: outOp = OpCode::LogicalOr; return true;
    case TokenType::KeywordIn: outOp = OpCode::In; return true;
    default: return false;
    }
}

bool tryExtractConstValue(const Expr& expr, Module& module, Value& outValue) {
    switch (expr.type) {
    case ExprType::Number:
        outValue = expr.value;
        return true;
    case ExprType::StringLiteral:
        outValue = Value::String(addString(module, expr.stringLiteral));
        return true;
    default:
        return false;
    }
}

std::string makeConstTempKey(const Value& value) {
    return std::to_string(static_cast<int>(value.type)) + ":" + std::to_string(value.payload);
}

std::size_t ensureConstTempLocalSlot(const Value& value,
                                     Module& module,
                                     std::unordered_map<std::string, std::size_t>& locals,
                                     FunctionIR& out,
                                     std::unordered_map<std::string, std::size_t>& constTempSlots) {
    const std::string key = makeConstTempKey(value);
    auto found = constTempSlots.find(key);
    if (found != constTempSlots.end()) {
        return found->second;
    }

    std::string tempName = "__gs_const_tmp_" + std::to_string(constTempSlots.size());
    while (locals.contains(tempName)) {
        tempName = "__gs_const_tmp_" + std::to_string(locals.size() + constTempSlots.size());
    }
    const std::size_t slot = ensureLocal(locals, out.localCount, tempName, &out);
    constTempSlots[key] = slot;

    emit(out.code,
         OpCode::LoadConst,
         addConstant(module, value),
         static_cast<std::int32_t>(slot));
    return slot;
}

void compileExpr(const Expr& expr,
                 Module& module,
                 const std::unordered_map<std::string, std::size_t>& locals,
                 const std::unordered_map<std::string, std::size_t>& funcIndex,
                 const std::unordered_map<std::string, std::size_t>& classIndex,
                 const std::string& currentFunctionName,
                 std::vector<IRInstruction>& code,
                 const std::unordered_map<std::string, std::size_t>* captureIndexByName = nullptr);

void compileStatements(const std::vector<Stmt>& statements,
                       Module& module,
                       std::unordered_map<std::string, std::size_t>& locals,
                       const std::unordered_map<std::string, std::size_t>& funcIndex,
                       const std::unordered_map<std::string, std::size_t>& classIndex,
                       const std::string& currentFunctionName,
                       bool isModuleInit,
                       FunctionIR& out,
                       LoopContext* loopContext,
                       std::unordered_map<std::string, std::size_t>& constTempSlots,
                       const std::unordered_map<std::string, std::size_t>* captureIndexByName = nullptr);

bool tryResolveOperandLocalSlot(const Expr& operand,
                                Module& module,
                                std::unordered_map<std::string, std::size_t>& locals,
                                const std::unordered_map<std::string, std::size_t>& funcIndex,
                                const std::unordered_map<std::string, std::size_t>& classIndex,
                                FunctionIR& out,
                                std::unordered_map<std::string, std::size_t>& constTempSlots,
                                std::size_t& outSlot) {
    Value constValue = Value::Nil();
    if (tryExtractConstValue(operand, module, constValue)) {
        outSlot = ensureConstTempLocalSlot(constValue, module, locals, out, constTempSlots);
        return true;
    }

    if (operand.type != ExprType::Variable) {
        return false;
    }

    const SymbolLookupResult resolved = resolveSymbol(locals,
                                                      module,
                                                      funcIndex,
                                                      classIndex,
                                                      operand.name);
    if (!resolved.found) {
        return false;
    }
    outSlot = resolved.localSlot;
    return true;
}

bool tryLowerBinaryExprToRegWithTempLocals(const Expr& expr,
                                           Module& module,
                                           std::unordered_map<std::string, std::size_t>& locals,
                                           const std::unordered_map<std::string, std::size_t>& funcIndex,
                                           const std::unordered_map<std::string, std::size_t>& classIndex,
                                           const std::string& currentFunctionName,
                                           FunctionIR& out,
                                           std::unordered_map<std::string, std::size_t>& constTempSlots,
                                           const std::unordered_map<std::string, std::size_t>* captureIndexByName = nullptr) {
    (void)constTempSlots;

    if (expr.type != ExprType::Binary && expr.type != ExprType::Unary) {
        return false;
    }

    std::unordered_map<std::string, std::size_t> constUseCounts;
    std::unordered_map<std::string, Value> constValues;
    std::function<void(const Expr&)> collectConstUsage = [&](const Expr& node) {
        Value constValue = Value::Nil();
        if (tryExtractConstValue(node, module, constValue)) {
            const std::string key = makeConstTempKey(constValue);
            ++constUseCounts[key];
            constValues[key] = constValue;
            return;
        }
        if (node.left) {
            collectConstUsage(*node.left);
        }
        if (node.right) {
            collectConstUsage(*node.right);
        }
    };
    collectConstUsage(expr);

    std::vector<std::size_t> reusableSlots;
    std::unordered_map<std::string, std::size_t> activeConstSlots;
    std::size_t exprTempOrdinal = 0;
    std::size_t constTempOrdinal = 0;

    auto acquireReusableSlot = [&](const std::string& prefix, std::size_t& ordinal) -> std::size_t {
        if (!reusableSlots.empty()) {
            const std::size_t slot = reusableSlots.back();
            reusableSlots.pop_back();
            if (out.localDebugNames.size() <= slot) {
                out.localDebugNames.resize(slot + 1);
            }
            out.localDebugNames[slot] = prefix + std::to_string(ordinal++);
            return slot;
        }

        std::string tempName = prefix + std::to_string(ordinal++);
        while (locals.contains(tempName)) {
            tempName = prefix + std::to_string(ordinal++);
        }
        return ensureLocal(locals, out.localCount, tempName, &out);
    };

    auto releaseReusableSlot = [&](std::size_t slot) {
        reusableSlots.push_back(slot);
    };

    auto acquireConstSlot = [&](const std::string& key) -> std::size_t {
        auto found = activeConstSlots.find(key);
        if (found != activeConstSlots.end()) {
            return found->second;
        }

        const std::size_t slot = acquireReusableSlot("__gs_const_tmp_", constTempOrdinal);
        const Value& value = constValues.at(key);
        emit(out.code,
             OpCode::LoadConst,
             addConstant(module, value),
             static_cast<std::int32_t>(slot));
        activeConstSlots[key] = slot;
        return slot;
    };

    auto consumeConstUse = [&](const std::string& key) {
        auto foundCount = constUseCounts.find(key);
        if (foundCount == constUseCounts.end()) {
            return;
        }
        if (foundCount->second == 0) {
            return;
        }
        --foundCount->second;
        if (foundCount->second != 0) {
            return;
        }

        constUseCounts.erase(foundCount);
        auto foundSlot = activeConstSlots.find(key);
        if (foundSlot != activeConstSlots.end()) {
            releaseReusableSlot(foundSlot->second);
            activeConstSlots.erase(foundSlot);
        }
    };

    struct LoweredValue {
        SlotType slotType{SlotType::Local};
        std::size_t slot{0};
        bool releasable{false};
        std::string constKey;
    };

    std::function<bool(const Expr&, bool, LoweredValue&)> lowerExprToLocalSlot;
    lowerExprToLocalSlot = [&](const Expr& node, bool requireStoredSlot, LoweredValue& lowered) -> bool {
        Value constValue = Value::Nil();
        if (tryExtractConstValue(node, module, constValue)) {
            lowered.slot = static_cast<std::size_t>(addConstant(module, constValue));
            lowered.slotType = SlotType::Constant;
            lowered.releasable = false;
            lowered.constKey.clear();
            return true;
        }

        if (node.type == ExprType::Variable) {
            if (captureIndexByName) {
                auto captureIt = captureIndexByName->find(node.name);
                if (captureIt != captureIndexByName->end()) {
                    lowered.slot = captureIt->second;
                    lowered.slotType = SlotType::UpValue;
                    lowered.releasable = false;
                    lowered.constKey.clear();
                    return true;
                }
            }
            const SymbolLookupResult resolved = resolveSymbol(locals,
                                                              module,
                                                              funcIndex,
                                                              classIndex,
                                                              node.name);
            if (resolved.found) {
                lowered.slot = resolved.localSlot;
                lowered.slotType = SlotType::Local;
                lowered.releasable = false;
                lowered.constKey.clear();
                return true;
            }
        }

        if (node.type == ExprType::Unary && node.right) {
            LoweredValue operand;
            if (!lowerExprToLocalSlot(*node.right, true, operand)) {
                return false;
            }

            switch (node.unaryOp) {
            case TokenType::Minus:
                emit(out.code,
                     OpCode::Negate,
                     static_cast<std::int32_t>(operand.slot),
                     0,
                     0,
                     0,
                     operand.slotType,
                     SlotType::None);
                break;
            case TokenType::Bang:
                emit(out.code,
                     OpCode::Not,
                     static_cast<std::int32_t>(operand.slot),
                     0,
                     0,
                     0,
                     operand.slotType,
                     SlotType::None);
                break;
            default:
                return false;
            }

            if (!operand.constKey.empty()) {
                consumeConstUse(operand.constKey);
            }
            if (operand.releasable) {
                releaseReusableSlot(operand.slot);
            }

            if (!requireStoredSlot) {
                lowered.slot = operand.slot;
                lowered.releasable = false;
                lowered.constKey.clear();
                return true;
            }

            const std::size_t tempSlot = acquireReusableSlot("__gs_expr_tmp_", exprTempOrdinal);
            emit(out.code,
                 OpCode::StoreLocalFromReg,
                 static_cast<std::int32_t>(tempSlot),
                 0);
              lowered.slotType = SlotType::Local;
            lowered.slot = tempSlot;
            lowered.releasable = true;
            lowered.constKey.clear();
            return true;
        }

        if (node.type == ExprType::Binary && node.left && node.right) {
            OpCode op = OpCode::Add;
            if (!tryGetBinaryOp(node.binaryOp, op)) {
                return false;
            }
            
            // Special handling for "not in" and "is not"
            if (node.binaryOp == TokenType::KeywordIn && node.unaryOp == TokenType::KeywordNot) {
                op = OpCode::NotIn;
            } else if (node.binaryOp == TokenType::KeywordIs && node.unaryOp == TokenType::KeywordNot) {
                op = OpCode::IsNot;
            }

            LoweredValue lhs;
            LoweredValue rhs;
            if (!lowerExprToLocalSlot(*node.left, true, lhs)) {
                return false;
            }
            if (!lowerExprToLocalSlot(*node.right, true, rhs)) {
                return false;
            }

            emit(out.code,
                 op,
                 static_cast<std::int32_t>(lhs.slot),
                  static_cast<std::int32_t>(rhs.slot),
                  0,
                  0,
                  lhs.slotType,
                  rhs.slotType);

            if (!lhs.constKey.empty()) {
                consumeConstUse(lhs.constKey);
            }
            if (!rhs.constKey.empty()) {
                consumeConstUse(rhs.constKey);
            }
            if (lhs.releasable) {
                releaseReusableSlot(lhs.slot);
            }
            if (rhs.releasable) {
                releaseReusableSlot(rhs.slot);
            }

            if (!requireStoredSlot) {
                lowered.slot = lhs.slot;
                lowered.releasable = false;
                lowered.constKey.clear();
                return true;
            }

            const std::size_t tempSlot = acquireReusableSlot("__gs_expr_tmp_", exprTempOrdinal);
            emit(out.code,
                 OpCode::StoreLocalFromReg,
                 static_cast<std::int32_t>(tempSlot),
                 0);
              lowered.slotType = SlotType::Local;
            lowered.slot = tempSlot;
            lowered.releasable = true;
            lowered.constKey.clear();
            return true;
        }

        const std::size_t tempSlot = acquireReusableSlot("__gs_expr_tmp_", exprTempOrdinal);
        compileExpr(node,
                    module,
                    locals,
                    funcIndex,
                    classIndex,
                    currentFunctionName,
                    out.code,
                    captureIndexByName);
        emit(out.code,
             OpCode::StoreLocal,
             static_cast<std::int32_t>(tempSlot),
             0);
           lowered.slotType = SlotType::Local;
        lowered.slot = tempSlot;
        lowered.releasable = true;
        lowered.constKey.clear();
        return true;
    };

    LoweredValue root;
    return lowerExprToLocalSlot(expr, false, root);
}

void compileExpr(const Expr& expr,
                 Module& module,
                 const std::unordered_map<std::string, std::size_t>& locals,
                 const std::unordered_map<std::string, std::size_t>& funcIndex,
                 const std::unordered_map<std::string, std::size_t>& classIndex,
                 const std::string& currentFunctionName,
                 std::vector<IRInstruction>& code,
                 const std::unordered_map<std::string, std::size_t>* captureIndexByName) {
    switch (expr.type) {
    case ExprType::Number:
        emit(code, OpCode::PushConst, addConstant(module, expr.value), 0);
        return;
    case ExprType::StringLiteral:
        emit(code,
             OpCode::PushConst,
             addConstant(module, Value::String(addString(module, expr.stringLiteral))),
             0);
        return;
    case ExprType::Variable: {
        if (captureIndexByName) {
            auto captureIt = captureIndexByName->find(expr.name);
            if (captureIt != captureIndexByName->end()) {
                emit(code, OpCode::PushCapture, static_cast<std::int32_t>(captureIt->second), 0);
                return;
            }
        }
        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          expr.name);
        if (resolved.found) {
            emitLocalValueToStack(code, resolved.localSlot);
        } else {
            emitNameValueToStack(code, addString(module, expr.name));
        }
        return;
    }
    case ExprType::Unary:
        if (!expr.right) {
            throw std::runtime_error(formatCompilerError("Unary expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        switch (expr.unaryOp) {
        case TokenType::Minus:
            emit(code, OpCode::Negate);
            break;
        case TokenType::Bang:
            emit(code, OpCode::Not);
            break;
        case TokenType::Tilde:
            emit(code, OpCode::BitwiseNot);
            break;
        default:
            throw std::runtime_error(formatCompilerError("Unsupported unary operator",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        return;
    case ExprType::AssignVariable: {
        if (!expr.right) {
            throw std::runtime_error(formatCompilerError("Variable assignment expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        const bool compiledToReg = tryCompileExprToRegister(*expr.right,
                                                            module,
                                                            locals,
                                                            funcIndex,
                                                            classIndex,
                                                            currentFunctionName,
                                                            code,
                                                            captureIndexByName);
        if (!compiledToReg) {
            compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        }

        if (captureIndexByName) {
            auto captureIt = captureIndexByName->find(expr.name);
            if (captureIt != captureIndexByName->end()) {
                if (compiledToReg) {
                    emit(code, OpCode::PushReg);
                }
                emit(code, OpCode::StoreCapture, static_cast<std::int32_t>(captureIt->second), 0);
                emit(code, OpCode::PushCapture, static_cast<std::int32_t>(captureIt->second), 0);
                return;
            }
        }

        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          expr.name);
        if (resolved.found) {
            if (compiledToReg) {
                emit(code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(resolved.localSlot), 0);
                emit(code, OpCode::PushReg);
            } else {
                emit(code, OpCode::StoreLocal, static_cast<std::int32_t>(resolved.localSlot), 0);
                emitLocalValueToStack(code, resolved.localSlot);
            }
        } else {
            const auto nameIdx = addString(module, expr.name);
            if (compiledToReg) {
                emit(code, OpCode::StoreNameFromReg, nameIdx, 0);
                emit(code, OpCode::PushReg);
            } else {
                emit(code, OpCode::StoreName, nameIdx, 0);
                emitNameValueToStack(code, nameIdx);
            }
        }
        return;
    }
    case ExprType::AssignProperty:
        if (!expr.object || !expr.right) {
            throw std::runtime_error(formatCompilerError("Property assignment expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        emit(code, OpCode::StoreAttr, addString(module, expr.propertyName), 0);
        return;
    case ExprType::AssignIndex:
        if (!expr.object || !expr.index || !expr.right) {
            throw std::runtime_error(formatCompilerError("Index assignment expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        compileExpr(*expr.index, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        emit(code, OpCode::CallMethod, addString(module, "set"), 2);
        return;
    case ExprType::Binary:
        if (tryCompileExprToRegister(expr,
                                     module,
                                     locals,
                                     funcIndex,
                                     classIndex,
                                     currentFunctionName,
                                     code,
                                     captureIndexByName)) {
            emit(code, OpCode::PushReg);
            return;
        }
        compileExpr(*expr.left, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        switch (expr.binaryOp) {
        case TokenType::Plus:
            emit(code, OpCode::Add);
            break;
        case TokenType::Minus:
            emit(code, OpCode::Sub);
            break;
        case TokenType::Star:
            emit(code, OpCode::Mul);
            break;
        case TokenType::Slash:
            emit(code, OpCode::Div);
            break;
        case TokenType::SlashSlash:
            emit(code, OpCode::FloorDiv);
            break;
        case TokenType::Percent:
            emit(code, OpCode::Mod);
            break;
        case TokenType::StarStar:
            emit(code, OpCode::Pow);
            break;
        case TokenType::Less:
            emit(code, OpCode::LessThan);
            break;
        case TokenType::Greater:
            emit(code, OpCode::GreaterThan);
            break;
        case TokenType::EqualEqual:
            emit(code, OpCode::Equal);
            break;
        case TokenType::BangEqual:
            // Check if this is "is not"
            if (expr.unaryOp == TokenType::KeywordNot) {
                emit(code, OpCode::IsNot);
            } else {
                emit(code, OpCode::NotEqual);
            }
            break;
        case TokenType::LessEqual:
            emit(code, OpCode::LessEqual);
            break;
        case TokenType::GreaterEqual:
            emit(code, OpCode::GreaterEqual);
            break;
        case TokenType::KeywordIs:
            emit(code, OpCode::Is);
            break;
        case TokenType::Amp:
            emit(code, OpCode::BitwiseAnd);
            break;
        case TokenType::Pipe:
            emit(code, OpCode::BitwiseOr);
            break;
        case TokenType::Caret:
            emit(code, OpCode::BitwiseXor);
            break;
        case TokenType::ShiftLeft:
            emit(code, OpCode::ShiftLeft);
            break;
        case TokenType::ShiftRight:
            emit(code, OpCode::ShiftRight);
            break;
        case TokenType::AmpAmp:
            emit(code, OpCode::LogicalAnd);
            break;
        case TokenType::PipePipe:
            emit(code, OpCode::LogicalOr);
            break;
        case TokenType::KeywordIn:
            // Check if this is "not in"
            if (expr.unaryOp == TokenType::KeywordNot) {
                emit(code, OpCode::NotIn);
            } else {
                emit(code, OpCode::In);
            }
            break;
        default:
            throw std::runtime_error(formatCompilerError("Unsupported binary operator",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        return;
    case ExprType::ListLiteral:
        for (const auto& it : expr.listElements) {
            compileExpr(it, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        }
        emit(code, OpCode::MakeList, static_cast<std::int32_t>(expr.listElements.size()));
        return;
    case ExprType::DictLiteral:
        for (const auto& kv : expr.dictEntries) {
            compileExpr(*kv.key, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
            compileExpr(*kv.value, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        }
        emit(code, OpCode::MakeDict, static_cast<std::int32_t>(expr.dictEntries.size()));
        return;
    case ExprType::Call: {
        if (!expr.callee) {
            throw std::runtime_error(formatCompilerError("Call expression callee is empty",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }

        if (expr.callee->type == ExprType::Variable) {
            const std::string& calleeName = expr.callee->name;

            if (captureIndexByName) {
                auto captureIt = captureIndexByName->find(calleeName);
                if (captureIt != captureIndexByName->end()) {
                    emit(code, OpCode::PushCapture, static_cast<std::int32_t>(captureIt->second), 0);
                    for (const auto& arg : expr.args) {
                        compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
                    }
                    emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
                    return;
                }
            }

            const SymbolLookupResult resolved = resolveSymbol(locals,
                                                              module,
                                                              funcIndex,
                                                              classIndex,
                                                              calleeName);

            if (!resolved.found) {
                emit(code, OpCode::LoadName, addString(module, calleeName), 0);
                for (const auto& arg : expr.args) {
                    compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
                }
                emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
                return;
            }

            emitLocalValueToStack(code, resolved.localSlot);
            for (const auto& arg : expr.args) {
                compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
            }
            emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
            return;
        }

        compileExpr(*expr.callee, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        for (const auto& arg : expr.args) {
            compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        }
        emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
        return;
    }
    case ExprType::MethodCall:
        if (!expr.object) {
            throw std::runtime_error(formatCompilerError("Method call object is empty",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        for (const auto& arg : expr.args) {
            compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        }
        emit(code,
             OpCode::CallMethod,
             addString(module, expr.methodName),
             static_cast<std::int32_t>(expr.args.size()));
        return;
    case ExprType::PropertyAccess:
        if (!expr.object) {
            throw std::runtime_error(formatCompilerError("Property access object is empty",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        emit(code, OpCode::LoadAttr, addString(module, expr.propertyName), 0);
        return;
    case ExprType::IndexAccess:
        if (!expr.object || !expr.index) {
            throw std::runtime_error(formatCompilerError("Index access expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        compileExpr(*expr.index, module, locals, funcIndex, classIndex, currentFunctionName, code, captureIndexByName);
        emit(code, OpCode::CallMethod, addString(module, "get"), 1);
        return;
    case ExprType::Lambda: {
        if (!expr.lambdaDecl) {
            throw std::runtime_error(formatCompilerError("Lambda declaration is missing",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        if (!g_mutableFuncIndex || !g_lambdaOrdinal || !g_allFunctionIrs) {
            throw std::runtime_error(formatCompilerError("Internal compiler lambda context is not initialized",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }

        std::unordered_set<std::string> lambdaLocals;
        for (const auto& param : expr.lambdaDecl->params) {
            lambdaLocals.insert(param);
        }
        collectLocalDeclarations(expr.lambdaDecl->body, lambdaLocals, currentFunctionName + "::<lambda>");

        std::vector<std::string> captureNames;
        std::unordered_set<std::string> captureDedup;
        collectCapturedNamesInStatements(expr.lambdaDecl->body,
                                         locals,
                                         lambdaLocals,
                                         captureNames,
                                         captureDedup);

        std::unordered_map<std::string, std::size_t> lambdaCaptureIndex;
        for (std::size_t i = 0; i < captureNames.size(); ++i) {
            lambdaCaptureIndex[captureNames[i]] = i;
        }

        const std::string lambdaName = "__lambda_" + std::to_string((*g_lambdaOrdinal)++);
        const std::size_t lambdaIndex = module.functions.size();
        (*g_mutableFuncIndex)[lambdaName] = lambdaIndex;

        FunctionBytecode lambdaBytecode;
        lambdaBytecode.name = lambdaName;
        lambdaBytecode.params = expr.lambdaDecl->params;
        lambdaBytecode.localCount = expr.lambdaDecl->params.size();
        module.functions.push_back(std::move(lambdaBytecode));

        FunctionIR lambdaIr;
        lambdaIr.name = lambdaName;
        lambdaIr.params = expr.lambdaDecl->params;
        lambdaIr.localCount = expr.lambdaDecl->params.size();

        std::unordered_map<std::string, std::size_t> lambdaLocalsMap;
        for (std::size_t i = 0; i < expr.lambdaDecl->params.size(); ++i) {
            lambdaLocalsMap[expr.lambdaDecl->params[i]] = i;
        }
        lambdaIr.localDebugNames = expr.lambdaDecl->params;

        std::unordered_map<std::string, std::size_t> lambdaConstTempSlots;
        compileStatements(expr.lambdaDecl->body,
                          module,
                          lambdaLocalsMap,
                          *g_mutableFuncIndex,
                          classIndex,
                          lambdaName,
                          false,
                          lambdaIr,
                          nullptr,
                          lambdaConstTempSlots,
                          &lambdaCaptureIndex);

        if (lambdaIr.code.empty() || lambdaIr.code.back().op != OpCode::Return) {
            emit(lambdaIr.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(lambdaIr.code, OpCode::Return);
        }

        g_allFunctionIrs->push_back(lambdaIr);
        module.functions[lambdaIndex] = lowerFunctionIR(lambdaIr);

        for (const auto& captureName : captureNames) {
            const auto localIt = locals.find(captureName);
            if (localIt == locals.end()) {
                continue;
            }
            emit(code, OpCode::CaptureLocal, static_cast<std::int32_t>(localIt->second), 0);
        }
        emit(code,
             OpCode::MakeClosure,
             static_cast<std::int32_t>(lambdaIndex),
             static_cast<std::int32_t>(captureNames.size()));
        return;
    }
    }
}

void compileStatements(const std::vector<Stmt>& statements,
                       Module& module,
                       std::unordered_map<std::string, std::size_t>& locals,
                       const std::unordered_map<std::string, std::size_t>& funcIndex,
                       const std::unordered_map<std::string, std::size_t>& classIndex,
                       const std::string& currentFunctionName,
                       bool isModuleInit,
                       FunctionIR& out,
                       LoopContext* loopContext,
                       std::unordered_map<std::string, std::size_t>& constTempSlots,
                       const std::unordered_map<std::string, std::size_t>* captureIndexByName) {
    auto compileCallLikeExprWithLoweredArgs = [&](const Expr& expr) -> bool {
        if (expr.type == ExprType::Call) {
            if (!expr.callee) {
                throw std::runtime_error(formatCompilerError("Call expression callee is empty",
                                                             currentFunctionName,
                                                             expr.line,
                                                             expr.column));
            }

            if (expr.callee->type == ExprType::Variable) {
                const std::string& calleeName = expr.callee->name;
                if (captureIndexByName && captureIndexByName->contains(calleeName)) {
                    emit(out.code,
                         OpCode::PushCapture,
                         static_cast<std::int32_t>(captureIndexByName->at(calleeName)),
                         0);
                } else {
                    const SymbolLookupResult resolved = resolveSymbol(locals,
                                                                      module,
                                                                      funcIndex,
                                                                      classIndex,
                                                                      calleeName);

                    if (!resolved.found) {
                        emit(out.code, OpCode::LoadName, addString(module, calleeName), 0);
                    } else {
                        emitLocalValueToStack(out.code, resolved.localSlot);
                    }
                }
            } else {
                compileExpr(*expr.callee,
                            module,
                            locals,
                            funcIndex,
                            classIndex,
                            currentFunctionName,
                            out.code,
                            captureIndexByName);
            }

            for (const auto& arg : expr.args) {
                if (tryLowerBinaryExprToRegWithTempLocals(arg,
                                                          module,
                                                          locals,
                                                          funcIndex,
                                                          classIndex,
                                                          currentFunctionName,
                                                          out,
                                                          constTempSlots,
                                                          captureIndexByName)) {
                    emit(out.code, OpCode::PushReg);
                } else {
                    compileExpr(arg,
                                module,
                                locals,
                                funcIndex,
                                classIndex,
                                currentFunctionName,
                                out.code,
                                captureIndexByName);
                }
            }

            emit(out.code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
            return true;
        }

        if (expr.type == ExprType::MethodCall) {
            if (!expr.object) {
                throw std::runtime_error(formatCompilerError("Method call object is empty",
                                                             currentFunctionName,
                                                             expr.line,
                                                             expr.column));
            }

            compileExpr(*expr.object,
                        module,
                        locals,
                        funcIndex,
                        classIndex,
                        currentFunctionName,
                        out.code,
                        captureIndexByName);

            for (const auto& arg : expr.args) {
                if (tryLowerBinaryExprToRegWithTempLocals(arg,
                                                          module,
                                                          locals,
                                                          funcIndex,
                                                          classIndex,
                                                          currentFunctionName,
                                                          out,
                                                          constTempSlots,
                                                          captureIndexByName)) {
                    emit(out.code, OpCode::PushReg);
                } else {
                    compileExpr(arg,
                                module,
                                locals,
                                funcIndex,
                                classIndex,
                                currentFunctionName,
                                out.code,
                                captureIndexByName);
                }
            }

            emit(out.code,
                 OpCode::CallMethod,
                 addString(module, expr.methodName),
                 static_cast<std::int32_t>(expr.args.size()));
            return true;
        }

        return false;
    };

    auto compileExprPreferLoweredBinary = [&](const Expr& expr) {
        if (tryLowerBinaryExprToRegWithTempLocals(expr,
                                                  module,
                                                  locals,
                                                  funcIndex,
                                                  classIndex,
                                                  currentFunctionName,
                                                  out,
                                                  constTempSlots)) {
            emit(out.code, OpCode::PushReg);
            return;
        }
        compileExpr(expr,
                    module,
                    locals,
                    funcIndex,
                    classIndex,
                    currentFunctionName,
                    out.code,
                    captureIndexByName);
    };

    std::size_t conditionTempOrdinal = 0;
    auto allocConditionTempSlot = [&]() -> std::size_t {
        std::string condName = "__gs_cond_tmp_" + std::to_string(conditionTempOrdinal++);
        while (locals.contains(condName)) {
            condName = "__gs_cond_tmp_" + std::to_string(conditionTempOrdinal++);
        }
        return ensureLocal(locals, out.localCount, condName, &out);
    };

    auto emitJumpIfConditionFalse = [&](const Expr& expr) -> std::size_t {
        if (tryLowerBinaryExprToRegWithTempLocals(expr,
                                                  module,
                                                  locals,
                                                  funcIndex,
                                                  classIndex,
                                                  currentFunctionName,
                                                  out,
                                                  constTempSlots)) {
            return emitJumpIfFalseReg(out.code);
        }

        if (tryCompileExprToRegister(expr,
                                     module,
                                     locals,
                                     funcIndex,
                                     classIndex,
                                     currentFunctionName,
                                     out.code,
                                     captureIndexByName)) {
            return emitJumpIfFalseReg(out.code);
        }

        compileExpr(expr,
                    module,
                    locals,
                    funcIndex,
                    classIndex,
                    currentFunctionName,
                    out.code,
                    captureIndexByName);
        const auto condSlot = allocConditionTempSlot();
        emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(condSlot), 0);
        emit(out.code, OpCode::MoveLocalToReg, static_cast<std::int32_t>(condSlot), 0);
        return emitJumpIfFalseReg(out.code);
    };

    for (const auto& stmt : statements) {
        switch (stmt.type) {
        case StmtType::LetExpr: {
            const bool loweredBinaryToReg = tryLowerBinaryExprToRegWithTempLocals(stmt.expr,
                                                                                   module,
                                                                                   locals,
                                                                                   funcIndex,
                                                                                   classIndex,
                                                                                   currentFunctionName,
                                                                                   out,
                                                                                   constTempSlots,
                                                                                   captureIndexByName);
            if (isModuleInit) {
                if (loweredBinaryToReg) {
                    emit(out.code, OpCode::StoreNameFromReg, addString(module, stmt.name), 0);
                } else if (tryCompileExprToRegister(stmt.expr,
                                             module,
                                             locals,
                                             funcIndex,
                                             classIndex,
                                             currentFunctionName,
                                             out.code,
                                             captureIndexByName)) {
                    emit(out.code, OpCode::StoreNameFromReg, addString(module, stmt.name), 0);
                } else {
                    compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
                    emit(out.code, OpCode::StoreName, addString(module, stmt.name), 0);
                }
            } else {
                if (locals.contains(stmt.name)) {
                    throw std::runtime_error(formatCompilerError("Duplicate let declaration in scope: " + stmt.name,
                                                                 currentFunctionName,
                                                                 stmt.line,
                                                                 stmt.column));
                }
                const auto slot = ensureLocal(locals, out.localCount, stmt.name, &out);
                Value letConstValue = Value::Nil();
                if (tryExtractConstValue(stmt.expr, module, letConstValue)) {
                    emit(out.code,
                         OpCode::LoadConst,
                         addConstant(module, letConstValue),
                         static_cast<std::int32_t>(slot));
                } else if (loweredBinaryToReg) {
                    emit(out.code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(slot));
                } else if (tryCompileExprToRegister(stmt.expr,
                                             module,
                                             locals,
                                             funcIndex,
                                             classIndex,
                                             currentFunctionName,
                                             out.code,
                                             captureIndexByName)) {
                    emit(out.code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(slot));
                } else {
                    compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
                    emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(slot));
                }
            }
            break;
        }
        case StmtType::LetSpawn: {
            throw std::runtime_error(formatCompilerError("'spawn' is temporarily disabled. Coroutine features are not enabled.",
                                                         currentFunctionName,
                                                         stmt.line,
                                                         stmt.column));
        }
        case StmtType::LetAwait: {
            throw std::runtime_error(formatCompilerError("'await' is temporarily disabled. Coroutine features are not enabled.",
                                                         currentFunctionName,
                                                         stmt.line,
                                                         stmt.column));
        }
        case StmtType::ForRange: {
            const auto iterSlot = ensureLocal(locals, out.localCount, stmt.iterKey, &out);
            const auto endSlot = ensureLocal(locals, out.localCount, "__for_end_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto oneSlot = ensureConstTempLocalSlot(Value::Int(1),
                                                          module,
                                                          locals,
                                                          out,
                                                          constTempSlots);

            compileExpr(stmt.rangeStart, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(iterSlot));
            compileExpr(stmt.rangeEnd, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(endSlot));

            const std::size_t loopStart = out.code.size();
              emit(out.code,
                  OpCode::LessThan,
                  static_cast<std::int32_t>(iterSlot),
                  static_cast<std::int32_t>(endSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              const std::size_t exitJump = emitJumpIfFalseReg(out.code);

            LoopContext localLoop;
            compileStatements(stmt.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              currentFunctionName,
                              isModuleInit,
                              out,
                              &localLoop,
                              constTempSlots,
                              captureIndexByName);

            localLoop.continueTarget = out.code.size();
              emit(out.code,
                  OpCode::Add,
                  static_cast<std::int32_t>(iterSlot),
                  static_cast<std::int32_t>(oneSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              emit(out.code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(iterSlot));
            emit(out.code, OpCode::Jump, static_cast<std::int32_t>(loopStart));

            const std::size_t loopEnd = out.code.size();
            patchJump(out.code, exitJump, loopEnd);
            for (auto jumpIndex : localLoop.continueJumps) {
                patchJump(out.code, jumpIndex, localLoop.continueTarget);
            }
            for (auto jumpIndex : localLoop.breakJumps) {
                patchJump(out.code, jumpIndex, loopEnd);
            }
            break;
        }
        case StmtType::ForList: {
            const auto itemSlot = ensureLocal(locals, out.localCount, stmt.iterKey, &out);
            const auto listSlot = ensureLocal(locals, out.localCount, "__for_list_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto indexSlot = ensureLocal(locals, out.localCount, "__for_idx_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto sizeSlot = ensureLocal(locals, out.localCount, "__for_size_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto oneSlot = ensureConstTempLocalSlot(Value::Int(1),
                                                          module,
                                                          locals,
                                                          out,
                                                          constTempSlots);

            compileExpr(stmt.iterable, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(listSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));

            const std::size_t loopStart = out.code.size();
            emitLocalValueToStack(out.code, listSlot);
            emit(out.code, OpCode::CallMethod, addString(module, "size"), 0);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(sizeSlot));

            emitLocalValueToStack(out.code, indexSlot);
            emitLocalValueToStack(out.code, sizeSlot);
              emit(out.code,
                  OpCode::LessThan,
                  static_cast<std::int32_t>(indexSlot),
                  static_cast<std::int32_t>(sizeSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              const std::size_t exitJump = emitJumpIfFalseReg(out.code);

            emitLocalValueToStack(out.code, listSlot);
            emitLocalValueToStack(out.code, indexSlot);
            emit(out.code, OpCode::CallMethod, addString(module, "get"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(itemSlot));

            LoopContext localLoop;
            compileStatements(stmt.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              currentFunctionName,
                              isModuleInit,
                              out,
                              &localLoop,
                              constTempSlots,
                              captureIndexByName);

            localLoop.continueTarget = out.code.size();
              emit(out.code,
                  OpCode::Add,
                  static_cast<std::int32_t>(indexSlot),
                  static_cast<std::int32_t>(oneSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              emit(out.code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::Jump, static_cast<std::int32_t>(loopStart));

            const std::size_t loopEnd = out.code.size();
            patchJump(out.code, exitJump, loopEnd);
            for (auto jumpIndex : localLoop.continueJumps) {
                patchJump(out.code, jumpIndex, localLoop.continueTarget);
            }
            for (auto jumpIndex : localLoop.breakJumps) {
                patchJump(out.code, jumpIndex, loopEnd);
            }
            break;
        }
        case StmtType::ForDict: {
            const auto keySlot = ensureLocal(locals, out.localCount, stmt.iterKey, &out);
            const auto valueSlot = ensureLocal(locals, out.localCount, stmt.iterValue, &out);
            const auto dictSlot = ensureLocal(locals, out.localCount, "__for_dict_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto indexSlot = ensureLocal(locals, out.localCount, "__for_idx_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto sizeSlot = ensureLocal(locals, out.localCount, "__for_size_" + stmt.iterKey + std::to_string(out.code.size()), &out);
            const auto oneSlot = ensureConstTempLocalSlot(Value::Int(1),
                                                          module,
                                                          locals,
                                                          out,
                                                          constTempSlots);

            compileExpr(stmt.iterable, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(dictSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));

            const std::size_t loopStart = out.code.size();
            emitLocalValueToStack(out.code, dictSlot);
            emit(out.code, OpCode::CallMethod, addString(module, "size"), 0);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(sizeSlot));

            emitLocalValueToStack(out.code, indexSlot);
            emitLocalValueToStack(out.code, sizeSlot);
              emit(out.code,
                  OpCode::LessThan,
                  static_cast<std::int32_t>(indexSlot),
                  static_cast<std::int32_t>(sizeSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              const std::size_t exitJump = emitJumpIfFalseReg(out.code);

            emitLocalValueToStack(out.code, dictSlot);
            emitLocalValueToStack(out.code, indexSlot);
            emit(out.code, OpCode::CallMethod, addString(module, "key_at"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(keySlot));

            emitLocalValueToStack(out.code, dictSlot);
            emitLocalValueToStack(out.code, indexSlot);
            emit(out.code, OpCode::CallMethod, addString(module, "value_at"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(valueSlot));

            LoopContext localLoop;
            compileStatements(stmt.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              currentFunctionName,
                              isModuleInit,
                              out,
                              &localLoop,
                              constTempSlots,
                              captureIndexByName);

            localLoop.continueTarget = out.code.size();
              emit(out.code,
                  OpCode::Add,
                  static_cast<std::int32_t>(indexSlot),
                  static_cast<std::int32_t>(oneSlot),
                  0,
                  0,
                  SlotType::Local,
                  SlotType::Local);
              emit(out.code, OpCode::StoreLocalFromReg, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::Jump, static_cast<std::int32_t>(loopStart));

            const std::size_t loopEnd = out.code.size();
            patchJump(out.code, exitJump, loopEnd);
            for (auto jumpIndex : localLoop.continueJumps) {
                patchJump(out.code, jumpIndex, localLoop.continueTarget);
            }
            for (auto jumpIndex : localLoop.breakJumps) {
                patchJump(out.code, jumpIndex, loopEnd);
            }
            break;
        }
        case StmtType::If: {
            std::vector<std::size_t> endJumps;
            for (std::size_t i = 0; i < stmt.branchConditions.size(); ++i) {
                const std::size_t falseJump = emitJumpIfConditionFalse(stmt.branchConditions[i]);
                compileStatements(stmt.branchBodies[i],
                                  module,
                                  locals,
                                  funcIndex,
                                  classIndex,
                                  currentFunctionName,
                                  isModuleInit,
                                  out,
                                  loopContext,
                                  constTempSlots,
                                  captureIndexByName);
                endJumps.push_back(emitJump(out.code, OpCode::Jump));
                patchJump(out.code, falseJump, out.code.size());
            }

            if (!stmt.elseBody.empty()) {
                compileStatements(stmt.elseBody,
                                  module,
                                  locals,
                                  funcIndex,
                                  classIndex,
                                  currentFunctionName,
                                  isModuleInit,
                                  out,
                                  loopContext,
                                  constTempSlots,
                                  captureIndexByName);
            }

            const std::size_t afterIf = out.code.size();
            for (auto jumpIndex : endJumps) {
                patchJump(out.code, jumpIndex, afterIf);
            }
            break;
        }
        case StmtType::While: {
            LoopContext localLoop;
            const std::size_t loopStart = out.code.size();
            localLoop.continueTarget = loopStart;

            const std::size_t exitJump = emitJumpIfConditionFalse(stmt.condition);

            compileStatements(stmt.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              currentFunctionName,
                              isModuleInit,
                              out,
                              &localLoop,
                              constTempSlots,
                              captureIndexByName);
            emit(out.code, OpCode::Jump, static_cast<std::int32_t>(loopStart));

            const std::size_t loopEnd = out.code.size();
            patchJump(out.code, exitJump, loopEnd);
            for (auto jumpIndex : localLoop.continueJumps) {
                patchJump(out.code, jumpIndex, localLoop.continueTarget);
            }
            for (auto jumpIndex : localLoop.breakJumps) {
                patchJump(out.code, jumpIndex, loopEnd);
            }
            break;
        }
        case StmtType::Break:
            if (!loopContext) {
                throw std::runtime_error(formatCompilerError("'break' used outside of loop",
                                                             currentFunctionName,
                                                             stmt.line,
                                                             stmt.column));
            }
            loopContext->breakJumps.push_back(emitJump(out.code, OpCode::Jump));
            break;
        case StmtType::Continue:
            if (!loopContext) {
                throw std::runtime_error(formatCompilerError("'continue' used outside of loop",
                                                             currentFunctionName,
                                                             stmt.line,
                                                             stmt.column));
            }
            loopContext->continueJumps.push_back(emitJump(out.code, OpCode::Jump));
            break;
        case StmtType::Expr:
            if (stmt.expr.type == ExprType::AssignVariable && stmt.expr.right) {
                const bool loweredAssignRhs = tryLowerBinaryExprToRegWithTempLocals(*stmt.expr.right,
                                                                                     module,
                                                                                     locals,
                                                                                     funcIndex,
                                                                                     classIndex,
                                                                                     currentFunctionName,
                                                                                     out,
                                                                                     constTempSlots,
                                                                                     captureIndexByName);
                if (loweredAssignRhs) {
                    if (captureIndexByName) {
                        auto captureIt = captureIndexByName->find(stmt.expr.name);
                        if (captureIt != captureIndexByName->end()) {
                            emit(out.code, OpCode::PushReg);
                            emit(out.code, OpCode::StoreCapture, static_cast<std::int32_t>(captureIt->second), 0);
                            break;
                        }
                    }
                    const SymbolLookupResult resolved = resolveSymbol(locals,
                                                                      module,
                                                                      funcIndex,
                                                                      classIndex,
                                                                      stmt.expr.name);
                    if (resolved.found) {
                        emit(out.code,
                             OpCode::StoreLocalFromReg,
                             static_cast<std::int32_t>(resolved.localSlot),
                             0);
                    } else {
                        emit(out.code, OpCode::StoreNameFromReg, addString(module, stmt.expr.name), 0);
                    }
                    break;
                }
            }

            if (stmt.expr.type == ExprType::AssignProperty && stmt.expr.object && stmt.expr.right) {
                compileExpr(*stmt.expr.object,
                            module,
                            locals,
                            funcIndex,
                            classIndex,
                            currentFunctionName,
                            out.code,
                            captureIndexByName);

                if (tryLowerBinaryExprToRegWithTempLocals(*stmt.expr.right,
                                                          module,
                                                          locals,
                                                          funcIndex,
                                                          classIndex,
                                                          currentFunctionName,
                                                          out,
                                                          constTempSlots,
                                                          captureIndexByName)) {
                    emit(out.code, OpCode::PushReg);
                } else {
                    compileExpr(*stmt.expr.right,
                                module,
                                locals,
                                funcIndex,
                                classIndex,
                                currentFunctionName,
                                out.code,
                                captureIndexByName);
                }

                emit(out.code, OpCode::StoreAttr, addString(module, stmt.expr.propertyName), 0);
                emit(out.code, OpCode::Pop);
                break;
            }

            if (stmt.expr.type == ExprType::AssignIndex && stmt.expr.object && stmt.expr.index && stmt.expr.right) {
                compileExpr(*stmt.expr.object,
                            module,
                            locals,
                            funcIndex,
                            classIndex,
                            currentFunctionName,
                            out.code,
                            captureIndexByName);
                compileExpr(*stmt.expr.index,
                            module,
                            locals,
                            funcIndex,
                            classIndex,
                            currentFunctionName,
                            out.code,
                            captureIndexByName);

                if (tryLowerBinaryExprToRegWithTempLocals(*stmt.expr.right,
                                                          module,
                                                          locals,
                                                          funcIndex,
                                                          classIndex,
                                                          currentFunctionName,
                                                          out,
                                                          constTempSlots,
                                                          captureIndexByName)) {
                    emit(out.code, OpCode::PushReg);
                } else {
                    compileExpr(*stmt.expr.right,
                                module,
                                locals,
                                funcIndex,
                                classIndex,
                                currentFunctionName,
                                out.code,
                                captureIndexByName);
                }

                emit(out.code, OpCode::CallMethod, addString(module, "set"), 2);
                emit(out.code, OpCode::Pop);
                break;
            }

            if (compileCallLikeExprWithLoweredArgs(stmt.expr)) {
                emit(out.code, OpCode::Pop);
                break;
            }

            compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            emit(out.code, OpCode::Pop);
            break;
        case StmtType::Return:
            if (tryLowerBinaryExprToRegWithTempLocals(stmt.expr,
                                                      module,
                                                      locals,
                                                      funcIndex,
                                                      classIndex,
                                                      currentFunctionName,
                                                      out,
                                                      constTempSlots,
                                                      captureIndexByName)) {
                emit(out.code, OpCode::PushReg);
            } else {
                compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code, captureIndexByName);
            }
            emit(out.code, OpCode::Return);
            break;
        case StmtType::Sleep:
            throw std::runtime_error(formatCompilerError("'sleep' is temporarily disabled. Coroutine features are not enabled.",
                                                         currentFunctionName,
                                                         stmt.line,
                                                         stmt.column));
        case StmtType::Yield:
            throw std::runtime_error(formatCompilerError("'yield' is temporarily disabled. Coroutine features are not enabled.",
                                                         currentFunctionName,
                                                         stmt.line,
                                                         stmt.column));
        }
    }
}

} // namespace

Module Compiler::compile(const Program& program) {
    lastFunctionIR_.clear();
    Module module;
    std::unordered_map<std::string, std::size_t> funcIndex;
    std::unordered_map<std::string, std::size_t> classIndex;
    std::size_t lambdaOrdinal = 0;
    struct LambdaContextGuard {
        std::unordered_map<std::string, std::size_t>* prevFuncIndex{nullptr};
        std::size_t* prevLambdaOrdinal{nullptr};
        std::vector<FunctionIR>* prevIrs{nullptr};
        ~LambdaContextGuard() {
            g_mutableFuncIndex = prevFuncIndex;
            g_lambdaOrdinal = prevLambdaOrdinal;
            g_allFunctionIrs = prevIrs;
        }
    } guard{g_mutableFuncIndex, g_lambdaOrdinal, g_allFunctionIrs};

    g_mutableFuncIndex = &funcIndex;
    g_lambdaOrdinal = &lambdaOrdinal;
    g_allFunctionIrs = &lastFunctionIR_;

    for (const auto& cls : program.classes) {
        if (classIndex.contains(cls.name)) {
            throw std::runtime_error(formatCompilerError("Duplicate class name: " + cls.name,
                                                         "<module>",
                                                         cls.line,
                                                         cls.column));
        }
        ClassBytecode classBc;
        classBc.name = cls.name;
        classIndex[cls.name] = module.classes.size();
        module.classes.push_back(std::move(classBc));
    }

    for (const auto& fn : program.functions) {
        if (funcIndex.contains(fn.name)) {
            throw std::runtime_error(formatCompilerError("Duplicate function name: " + fn.name,
                                                         "<module>",
                                                         fn.line,
                                                         fn.column));
        }
        FunctionBytecode compiled;
        compiled.name = fn.name;
        compiled.params = fn.params;
        compiled.localCount = fn.params.size();

        funcIndex[fn.name] = module.functions.size();
        module.functions.push_back(std::move(compiled));
    }

    const std::string moduleInitName = "__module_init__";
    if (!funcIndex.contains(moduleInitName)) {
        FunctionBytecode moduleInit;
        moduleInit.name = moduleInitName;
        moduleInit.localCount = 0;
        funcIndex[moduleInitName] = module.functions.size();
        module.functions.push_back(std::move(moduleInit));
    }

    std::unordered_set<std::string> declaredModuleGlobals;
    for (const auto& stmt : program.topLevelStatements) {
        if (stmt.type != StmtType::LetExpr) {
            continue;
        }

        if (funcIndex.contains(stmt.name) || classIndex.contains(stmt.name)) {
            throw std::runtime_error(formatCompilerError("Duplicate top-level symbol name: " + stmt.name,
                                                         "<module>",
                                                         stmt.line,
                                                         stmt.column));
        }

        if (!declaredModuleGlobals.contains(stmt.name)) {
            module.globals.push_back({stmt.name, Value::Nil()});
            declaredModuleGlobals.insert(stmt.name);
        }
    }

    for (const auto& cls : program.classes) {
        auto& classBc = module.classes[classIndex.at(cls.name)];
        if (!cls.baseName.empty()) {
            auto it = classIndex.find(cls.baseName);
            if (it == classIndex.end()) {
                throw std::runtime_error(formatCompilerError("Unknown base class: " + cls.baseName,
                                                             "<module>",
                                                             cls.line,
                                                             cls.column));
            }
            classBc.baseClassIndex = static_cast<std::int32_t>(it->second);
        }

        for (const auto& attr : cls.attributes) {
            classBc.attributes.push_back({attr.name, evalClassFieldInit(attr.initializer,
                                                                        module,
                                                                        funcIndex,
                                                                        classIndex,
                                                                        cls.name + "::<attr>" )});
        }

        bool hasCtor = false;
        for (const auto& method : cls.methods) {
            if (method.name == "__new__") {
                hasCtor = true;
                if (method.params.empty()) {
                    throw std::runtime_error(formatCompilerError("Class constructor __new__ must declare self parameter: " + cls.name,
                                                                 cls.name + "::__new__",
                                                                 method.line,
                                                                 method.column));
                }
            }

            const std::string mangled = mangleMethodName(cls.name, method.name);
            if (funcIndex.contains(mangled)) {
                throw std::runtime_error(formatCompilerError("Duplicate method: " + mangled,
                                                             mangled,
                                                             method.line,
                                                             method.column));
            }
            FunctionBytecode compiled;
            compiled.name = mangled;
            compiled.params = method.params;
            compiled.localCount = method.params.size();
            const std::size_t idx = module.functions.size();
            funcIndex[mangled] = idx;
            module.functions.push_back(std::move(compiled));
            classBc.methods.push_back({method.name, idx});
        }

        if (!hasCtor) {
            throw std::runtime_error(formatCompilerError("Class must define constructor __new__: " + cls.name,
                                                         cls.name,
                                                         cls.line,
                                                         cls.column));
        }
    }

    for (const auto& fn : program.functions) {
        const std::size_t functionIndex = funcIndex.at(fn.name);
        FunctionBytecode& out = module.functions[functionIndex];
        validateScopeLocalRules(fn.body, out.params, fn.name);
        FunctionIR functionIr;
        functionIr.name = out.name;
        functionIr.params = out.params;
        functionIr.localCount = out.params.size();
        std::unordered_map<std::string, std::size_t> locals;
        for (std::size_t i = 0; i < functionIr.params.size(); ++i) {
            locals[functionIr.params[i]] = i;
        }
        functionIr.localDebugNames = functionIr.params;
        std::unordered_map<std::string, std::size_t> constTempSlots;
        compileStatements(fn.body,
                          module,
                          locals,
                          funcIndex,
                          classIndex,
                          fn.name,
                          false,
                          functionIr,
                          nullptr,
                          constTempSlots);

        if (functionIr.code.empty() || functionIr.code.back().op != OpCode::Return) {
            emit(functionIr.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(functionIr.code, OpCode::Return);
        }

        lastFunctionIR_.push_back(functionIr);
        module.functions[functionIndex] = lowerFunctionIR(functionIr);
    }

    for (const auto& cls : program.classes) {
        for (const auto& method : cls.methods) {
            const std::string mangled = mangleMethodName(cls.name, method.name);
            const std::size_t functionIndex = funcIndex.at(mangled);
            FunctionBytecode& out = module.functions[functionIndex];
            validateScopeLocalRules(method.body, out.params, cls.name + "::" + method.name);
            FunctionIR functionIr;
            functionIr.name = out.name;
            functionIr.params = out.params;
            functionIr.localCount = out.params.size();
            std::unordered_map<std::string, std::size_t> locals;
            for (std::size_t i = 0; i < functionIr.params.size(); ++i) {
                locals[functionIr.params[i]] = i;
            }
            functionIr.localDebugNames = functionIr.params;
            std::unordered_map<std::string, std::size_t> constTempSlots;

            compileStatements(method.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              cls.name + "::" + method.name,
                              false,
                              functionIr,
                              nullptr,
                              constTempSlots);

            if (functionIr.code.empty() || functionIr.code.back().op != OpCode::Return) {
                emit(functionIr.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
                emit(functionIr.code, OpCode::Return);
            }

            lastFunctionIR_.push_back(functionIr);
            module.functions[functionIndex] = lowerFunctionIR(functionIr);
        }
    }

    {
        const std::size_t functionIndex = funcIndex.at(moduleInitName);
        FunctionBytecode& out = module.functions[functionIndex];
        validateScopeLocalRules(program.topLevelStatements, {}, moduleInitName);
        FunctionIR functionIr;
        functionIr.name = out.name;
        functionIr.params = out.params;
        functionIr.localCount = out.localCount;
        std::unordered_map<std::string, std::size_t> locals;
        std::unordered_map<std::string, std::size_t> constTempSlots;
        compileStatements(program.topLevelStatements,
                          module,
                          locals,
                          funcIndex,
                          classIndex,
                          moduleInitName,
                          true,
                          functionIr,
                  nullptr,
                  constTempSlots);
        if (functionIr.code.empty() || functionIr.code.back().op != OpCode::Return) {
            emit(functionIr.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(functionIr.code, OpCode::Return);
        }
        lastFunctionIR_.push_back(functionIr);
        module.functions[functionIndex] = lowerFunctionIR(functionIr);
    }

    return module;
}

const std::vector<FunctionIR>& Compiler::lastFunctionIR() const {
    return lastFunctionIR_;
}

Module compileSource(const std::string& source) {
    Tokenizer tokenizer(source);
    Parser parser(tokenizer.tokenize());
    Compiler compiler;
    return compiler.compile(parser.parseProgram());
}

void dumpTransformedSourceFile(const std::string& sourcePath, const std::string& transformedSource) {
    namespace fs = std::filesystem;
    fs::path outputPath(sourcePath);
    outputPath.replace_extension(".gst");

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error("error: failed to dump transformed source to " + outputPath.string() + " [function: <module>]");
    }
    output << transformedSource;
    if (!output) {
        throw std::runtime_error("error: failed to write transformed source to " + outputPath.string() + " [function: <module>]");
    }
}

std::string inferFunctionNameAtLine(const std::string& source, std::size_t lineNo) {
    static const std::regex fnHeaderRe(R"(^\s*fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{)");
    const auto lines = splitLines(source);
    if (lineNo == 0 || lineNo > lines.size()) {
        return "<module>";
    }

    std::string currentFunction = "<module>";
    int depth = 0;
    int functionDepth = 0;

    for (std::size_t i = 0; i < lineNo; ++i) {
        const auto& line = lines[i];
        std::smatch match;
        bool functionStartsHere = std::regex_search(line, match, fnHeaderRe);

        for (char c : line) {
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
            }
        }

        if (functionStartsHere) {
            currentFunction = match[1].str();
            functionDepth = depth;
        }

        if (currentFunction != "<module>" && depth < functionDepth) {
            currentFunction = "<module>";
            functionDepth = 0;
        }
    }

    return currentFunction;
}

std::string tryFillFunctionContext(const std::string& diagnostic, const std::string& source) {
    if (diagnostic.find("[function: <module>]") == std::string::npos) {
        return diagnostic;
    }

    std::smatch match;
    static const std::regex lineColRe(R"(^\s*(\d+):(\d+):\s*error:.*\[function:\s*<module>\]\s*$)");
    if (!std::regex_match(diagnostic, match, lineColRe)) {
        return diagnostic;
    }

    const std::size_t lineNo = static_cast<std::size_t>(std::stoull(match[1].str()));
    const std::string inferredFunction = inferFunctionNameAtLine(source, lineNo);
    if (inferredFunction == "<module>") {
        return diagnostic;
    }

    std::string updated = diagnostic;
    const std::string oldTag = "[function: <module>]";
    const std::string newTag = "[function: " + inferredFunction + "]";
    const auto pos = updated.find(oldTag);
    if (pos != std::string::npos) {
        updated.replace(pos, oldTag.size(), newTag);
    }
    return updated;
}

Module compileSourceFile(const std::string& path,
                         const std::vector<std::string>& searchPaths,
                         bool dumpTransformedSource) {
    std::string mergedSource;
    try {
        std::unordered_map<std::string, ProcessedModule> cache;
        std::unordered_set<std::string> visiting;
        const auto processed = preprocessImportsRecursive(path, searchPaths, cache, visiting);
        mergedSource = processed.source;
        if (dumpTransformedSource) {
            dumpTransformedSourceFile(path, mergedSource);
        }
        Tokenizer tokenizer(mergedSource);
        Parser parser(tokenizer.tokenize());
        Compiler compiler;
        Module module = compiler.compile(parser.parseProgram());
        if (g_compileDisassemblyDumpEnabled) {
            dumpCompilerDebugFiles(path, module, compiler.lastFunctionIR());
        }
        return module;
    } catch (const std::exception& ex) {
        throw std::runtime_error(path + ":" + tryFillFunctionContext(ex.what(), mergedSource));
    }
}

void setCompileDisassemblyDumpEnabled(bool enabled) {
    g_compileDisassemblyDumpEnabled = enabled;
}

bool compileDisassemblyDumpEnabled() {
    return g_compileDisassemblyDumpEnabled;
}

std::string serializeModuleText(const Module& module) {
    std::ostringstream out;
    out << "GSBC1\n";
    out << module.constants.size() << "\n";
    for (auto c : module.constants) {
        out << static_cast<int>(c.type) << ' ' << c.payload << "\n";
    }

    out << module.strings.size() << "\n";
    for (const auto& s : module.strings) {
        out << std::quoted(s) << "\n";
    }

    out << module.functions.size() << "\n";
    for (const auto& fn : module.functions) {
        out << std::quoted(fn.name) << "\n";
        out << fn.params.size() << "\n";
        for (const auto& p : fn.params) {
            out << std::quoted(p) << "\n";
        }
        out << fn.localCount << "\n";
        out << fn.stackSlotCount << "\n";
        out << fn.code.size() << "\n";
        for (const auto& ins : fn.code) {
            out << static_cast<int>(ins.op) << ' '
                << static_cast<int>(ins.aSlotType) << ' ' << ins.a << ' '
                << static_cast<int>(ins.bSlotType) << ' ' << ins.b << "\n";
        }
    }

    out << module.classes.size() << "\n";
    for (const auto& cls : module.classes) {
        out << std::quoted(cls.name) << "\n";
        out << cls.baseClassIndex << "\n";
        out << cls.attributes.size() << "\n";
        for (const auto& attr : cls.attributes) {
            out << std::quoted(attr.name) << " " << static_cast<int>(attr.defaultValue.type) << " "
                << attr.defaultValue.payload << "\n";
        }
        out << cls.methods.size() << "\n";
        for (const auto& method : cls.methods) {
            out << std::quoted(method.name) << " " << method.functionIndex << "\n";
        }
    }

    out << module.globals.size() << "\n";
    for (const auto& global : module.globals) {
        out << std::quoted(global.name) << " " << static_cast<int>(global.initialValue.type)
            << " " << global.initialValue.payload << "\n";
    }

    return out.str();
}

Module deserializeModuleText(const std::string& text) {
    std::istringstream in(text);
    std::string magic;
    std::getline(in, magic);
    if (magic != "GSBC1") {
        throw std::runtime_error("Invalid bytecode header");
    }

    Module module;
    std::size_t count = 0;

    in >> count;
    for (std::size_t i = 0; i < count; ++i) {
        int t = 0;
        Value v;
        in >> t >> v.payload;
        v.type = static_cast<ValueType>(t);
        module.constants.push_back(v);
    }

    in >> count;
    for (std::size_t i = 0; i < count; ++i) {
        std::string s;
        in >> std::quoted(s);
        module.strings.push_back(s);
    }

    in >> count;
    for (std::size_t i = 0; i < count; ++i) {
        FunctionBytecode fn;
        in >> std::quoted(fn.name);

        std::size_t paramCount = 0;
        in >> paramCount;
        fn.params.reserve(paramCount);
        for (std::size_t p = 0; p < paramCount; ++p) {
            std::string param;
            in >> std::quoted(param);
            fn.params.push_back(std::move(param));
        }

        in >> fn.localCount;
    in >> fn.stackSlotCount;

        std::size_t codeCount = 0;
        in >> codeCount;
        fn.code.reserve(codeCount);
        for (std::size_t c = 0; c < codeCount; ++c) {
            int op = 0;
            int aSlot = 0;
            int bSlot = 0;
            std::int32_t a = 0;
            std::int32_t b = 0;
            Instruction ins;
            in >> op >> aSlot >> a >> bSlot >> b;
            ins.op = static_cast<OpCode>(op);
            ins.aSlotType = static_cast<SlotType>(aSlot);
            ins.a = a;
            ins.bSlotType = static_cast<SlotType>(bSlot);
            ins.b = b;
            fn.code.push_back(ins);
        }

        module.functions.push_back(std::move(fn));
    }

    in >> count;
    for (std::size_t i = 0; i < count; ++i) {
        ClassBytecode cls;
        in >> std::quoted(cls.name);
        in >> cls.baseClassIndex;

        std::size_t attrCount = 0;
        in >> attrCount;
        cls.attributes.reserve(attrCount);
        for (std::size_t a = 0; a < attrCount; ++a) {
            ClassAttributeBinding attr;
            int type = 0;
            in >> std::quoted(attr.name) >> type >> attr.defaultValue.payload;
            attr.defaultValue.type = static_cast<ValueType>(type);
            cls.attributes.push_back(std::move(attr));
        }

        std::size_t methodCount = 0;
        in >> methodCount;
        cls.methods.reserve(methodCount);
        for (std::size_t m = 0; m < methodCount; ++m) {
            ClassMethodBinding binding;
            in >> std::quoted(binding.name) >> binding.functionIndex;
            cls.methods.push_back(std::move(binding));
        }

        module.classes.push_back(std::move(cls));
    }

    in >> count;
    for (std::size_t i = 0; i < count; ++i) {
        GlobalBinding global;
        int type = 0;
        in >> std::quoted(global.name) >> type >> global.initialValue.payload;
        global.initialValue.type = static_cast<ValueType>(type);
        module.globals.push_back(std::move(global));
    }

    return module;
}

std::string generateAotCpp(const Module& module, const std::string& variableName) {
    std::ostringstream out;
    out << "#include \"gs/bytecode.hpp\"\n\n";
    out << "gs::Module " << variableName << "() {\n";
    out << "    gs::Module m;\n";

    for (auto c : module.constants) {
        out << "    m.constants.push_back(";
        switch (c.type) {
        case ValueType::Nil:
            out << "gs::Value::Nil()";
            break;
        case ValueType::Int:
            out << "gs::Value::Int(" << c.payload << ")";
            break;
        case ValueType::Float:
            out << "gs::Value::Float(" << std::setprecision(17) << c.asFloat() << ")";
            break;
        case ValueType::String:
            out << "gs::Value::String(" << c.payload << ")";
            break;
        case ValueType::Ref:
            throw std::runtime_error("AOT generation does not support runtime Ref values in constants");
        case ValueType::Function:
            out << "gs::Value::Function(" << c.payload << ")";
            break;
        case ValueType::Class:
            out << "gs::Value::Class(" << c.payload << ")";
            break;
        case ValueType::Module:
            out << "gs::Value::Module(" << c.payload << ")";
            break;
        }
        out << ");\n";
    }
    for (const auto& s : module.strings) {
        out << "    m.strings.push_back(" << std::quoted(s) << ");\n";
    }

    for (const auto& fn : module.functions) {
        out << "    {\n";
        out << "        gs::FunctionBytecode f;\n";
        out << "        f.name = " << std::quoted(fn.name) << ";\n";
        for (const auto& p : fn.params) {
            out << "        f.params.push_back(" << std::quoted(p) << ");\n";
        }
        out << "        f.localCount = " << fn.localCount << ";\n";
        out << "        f.stackSlotCount = " << fn.stackSlotCount << ";\n";
        for (const auto& ins : fn.code) {
            out << "        f.code.push_back(gs::Instruction{gs::OpCode::";
            out << opcodeName(ins.op);
            out << ", " << ins.a << ", " << ins.b << "});\n";
        }
        out << "        m.functions.push_back(std::move(f));\n";
        out << "    }\n";
    }

    for (const auto& cls : module.classes) {
        out << "    {\n";
        out << "        gs::ClassBytecode c;\n";
        out << "        c.name = " << std::quoted(cls.name) << ";\n";
        out << "        c.baseClassIndex = " << cls.baseClassIndex << ";\n";
        for (const auto& attr : cls.attributes) {
            out << "        c.attributes.push_back(gs::ClassAttributeBinding{" << std::quoted(attr.name)
                << ", ";
            switch (attr.defaultValue.type) {
            case ValueType::Nil:
                out << "gs::Value::Nil()";
                break;
            case ValueType::Int:
                out << "gs::Value::Int(" << attr.defaultValue.payload << ")";
                break;
            case ValueType::Float:
                out << "gs::Value::Float(" << std::setprecision(17) << attr.defaultValue.asFloat() << ")";
                break;
            case ValueType::String:
                out << "gs::Value::String(" << attr.defaultValue.payload << ")";
                break;
            case ValueType::Ref:
                throw std::runtime_error("AOT generation does not support runtime Ref values in class attributes");
                break;
            case ValueType::Function:
                out << "gs::Value::Function(" << attr.defaultValue.payload << ")";
                break;
            case ValueType::Class:
                out << "gs::Value::Class(" << attr.defaultValue.payload << ")";
                break;
            case ValueType::Module:
                out << "gs::Value::Module(" << attr.defaultValue.payload << ")";
                break;
            }
            out << "});\n";
        }
        for (const auto& method : cls.methods) {
            out << "        c.methods.push_back(gs::ClassMethodBinding{" << std::quoted(method.name)
                << ", " << method.functionIndex << "});\n";
        }
        out << "        m.classes.push_back(std::move(c));\n";
        out << "    }\n";
    }

    for (const auto& global : module.globals) {
        out << "    m.globals.push_back(gs::GlobalBinding{" << std::quoted(global.name)
            << ", ";
        switch (global.initialValue.type) {
        case ValueType::Nil:
            out << "gs::Value::Nil()";
            break;
        case ValueType::Int:
            out << "gs::Value::Int(" << global.initialValue.payload << ")";
            break;
        case ValueType::Float:
            out << "gs::Value::Float(" << std::setprecision(17) << global.initialValue.asFloat() << ")";
            break;
        case ValueType::String:
            out << "gs::Value::String(" << global.initialValue.payload << ")";
            break;
        case ValueType::Ref:
            throw std::runtime_error("AOT generation does not support runtime Ref values in globals");
            break;
        case ValueType::Function:
            out << "gs::Value::Function(" << global.initialValue.payload << ")";
            break;
        case ValueType::Class:
            out << "gs::Value::Class(" << global.initialValue.payload << ")";
            break;
        case ValueType::Module:
            out << "gs::Value::Module(" << global.initialValue.payload << ")";
            break;
        }
        out << "});\n";
    }

    out << "    return m;\n";
    out << "}\n";
    return out.str();
}

} // namespace gs
