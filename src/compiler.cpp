#include "gs/compiler.hpp"

#include "gs/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
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

void emit(std::vector<Instruction>& code, OpCode op, std::int32_t a = 0, std::int32_t b = 0) {
    code.push_back({op, a, b});
}

std::size_t emitJump(std::vector<Instruction>& code, OpCode op) {
    code.push_back({op, -1, 0});
    return code.size() - 1;
}

void patchJump(std::vector<Instruction>& code, std::size_t jumpIndex, std::size_t targetIndex) {
    code[jumpIndex].a = static_cast<std::int32_t>(targetIndex);
}

std::size_t ensureLocal(std::unordered_map<std::string, std::size_t>& locals,
                        FunctionBytecode& fn,
                        const std::string& name) {
    auto it = locals.find(name);
    if (it != locals.end()) {
        return it->second;
    }
    const std::size_t slot = fn.localCount;
    locals[name] = slot;
    ++fn.localCount;
    return slot;
}

void compileExpr(const Expr& expr,
                 Module& module,
                 const std::unordered_map<std::string, std::size_t>& locals,
                 const std::unordered_map<std::string, std::size_t>& funcIndex,
                 const std::unordered_map<std::string, std::size_t>& classIndex,
                 const std::string& currentFunctionName,
                 std::vector<Instruction>& code) {
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
        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          expr.name);
        if (!resolved.found) {
            emit(code, OpCode::LoadName, addString(module, expr.name), 0);
            return;
        }

        emit(code, OpCode::LoadLocal, static_cast<std::int32_t>(resolved.localSlot), 0);
        return;
    }
    case ExprType::AssignVariable: {
        if (!expr.right) {
            throw std::runtime_error(formatCompilerError("Variable assignment expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code);
        const SymbolLookupResult resolved = resolveSymbol(locals,
                                                          module,
                                                          funcIndex,
                                                          classIndex,
                                                          expr.name);
        if (resolved.found) {
            emit(code, OpCode::StoreLocal, static_cast<std::int32_t>(resolved.localSlot), 0);
            emit(code, OpCode::LoadLocal, static_cast<std::int32_t>(resolved.localSlot), 0);
        } else {
            emit(code, OpCode::StoreName, addString(module, expr.name), 0);
            emit(code, OpCode::LoadName, addString(module, expr.name), 0);
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
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code);
        emit(code, OpCode::StoreAttr, addString(module, expr.propertyName), 0);
        return;
    case ExprType::AssignIndex:
        if (!expr.object || !expr.index || !expr.right) {
            throw std::runtime_error(formatCompilerError("Index assignment expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code);
        compileExpr(*expr.index, module, locals, funcIndex, classIndex, currentFunctionName, code);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code);
        emit(code, OpCode::CallMethod, addString(module, "set"), 2);
        return;
    case ExprType::Binary:
        compileExpr(*expr.left, module, locals, funcIndex, classIndex, currentFunctionName, code);
        compileExpr(*expr.right, module, locals, funcIndex, classIndex, currentFunctionName, code);
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
            emit(code, OpCode::NotEqual);
            break;
        case TokenType::LessEqual:
            emit(code, OpCode::LessEqual);
            break;
        case TokenType::GreaterEqual:
            emit(code, OpCode::GreaterEqual);
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
            compileExpr(it, module, locals, funcIndex, classIndex, currentFunctionName, code);
        }
        emit(code, OpCode::MakeList, static_cast<std::int32_t>(expr.listElements.size()));
        return;
    case ExprType::DictLiteral:
        for (const auto& kv : expr.dictEntries) {
            compileExpr(*kv.key, module, locals, funcIndex, classIndex, currentFunctionName, code);
            compileExpr(*kv.value, module, locals, funcIndex, classIndex, currentFunctionName, code);
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

            const SymbolLookupResult resolved = resolveSymbol(locals,
                                                              module,
                                                              funcIndex,
                                                              classIndex,
                                                              calleeName);

            if (!resolved.found) {
                emit(code, OpCode::LoadName, addString(module, calleeName), 0);
                for (const auto& arg : expr.args) {
                    compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code);
                }
                emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
                return;
            }

            emit(code, OpCode::LoadLocal, static_cast<std::int32_t>(resolved.localSlot), 0);
            for (const auto& arg : expr.args) {
                compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code);
            }
            emit(code, OpCode::CallValue, static_cast<std::int32_t>(expr.args.size()), 0);
            return;
        }

        compileExpr(*expr.callee, module, locals, funcIndex, classIndex, currentFunctionName, code);
        for (const auto& arg : expr.args) {
            compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code);
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
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code);
        for (const auto& arg : expr.args) {
            compileExpr(arg, module, locals, funcIndex, classIndex, currentFunctionName, code);
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
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code);
        emit(code, OpCode::LoadAttr, addString(module, expr.propertyName), 0);
        return;
    case ExprType::IndexAccess:
        if (!expr.object || !expr.index) {
            throw std::runtime_error(formatCompilerError("Index access expression is incomplete",
                                                         currentFunctionName,
                                                         expr.line,
                                                         expr.column));
        }
        compileExpr(*expr.object, module, locals, funcIndex, classIndex, currentFunctionName, code);
        compileExpr(*expr.index, module, locals, funcIndex, classIndex, currentFunctionName, code);
        emit(code, OpCode::CallMethod, addString(module, "get"), 1);
        return;
    }
}

void compileStatements(const std::vector<Stmt>& statements,
                       Module& module,
                       std::unordered_map<std::string, std::size_t>& locals,
                       const std::unordered_map<std::string, std::size_t>& funcIndex,
                       const std::unordered_map<std::string, std::size_t>& classIndex,
                       const std::string& currentFunctionName,
                       bool isModuleInit,
                       FunctionBytecode& out,
                       LoopContext* loopContext) {
    for (const auto& stmt : statements) {
        switch (stmt.type) {
        case StmtType::LetExpr: {
            compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            if (isModuleInit) {
                emit(out.code, OpCode::StoreName, addString(module, stmt.name), 0);
            } else {
                if (locals.contains(stmt.name)) {
                    throw std::runtime_error(formatCompilerError("Duplicate let declaration in scope: " + stmt.name,
                                                                 currentFunctionName,
                                                                 stmt.line,
                                                                 stmt.column));
                }
                const auto slot = ensureLocal(locals, out, stmt.name);
                emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(slot));
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
            const auto iterSlot = ensureLocal(locals, out, stmt.iterKey);
            const auto endSlot = ensureLocal(locals, out, "__for_end_" + stmt.iterKey + std::to_string(out.code.size()));

            compileExpr(stmt.rangeStart, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(iterSlot));
            compileExpr(stmt.rangeEnd, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(endSlot));

            const std::size_t loopStart = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(iterSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(endSlot));
            emit(out.code, OpCode::LessThan);
            const std::size_t exitJump = emitJump(out.code, OpCode::JumpIfFalse);

            LoopContext localLoop;
            compileStatements(stmt.body, module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, &localLoop);

            localLoop.continueTarget = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(iterSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(1)));
            emit(out.code, OpCode::Add);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(iterSlot));
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
            const auto itemSlot = ensureLocal(locals, out, stmt.iterKey);
            const auto listSlot = ensureLocal(locals, out, "__for_list_" + stmt.iterKey + std::to_string(out.code.size()));
            const auto indexSlot = ensureLocal(locals, out, "__for_idx_" + stmt.iterKey + std::to_string(out.code.size()));
            const auto sizeSlot = ensureLocal(locals, out, "__for_size_" + stmt.iterKey + std::to_string(out.code.size()));

            compileExpr(stmt.iterable, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(listSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));

            const std::size_t loopStart = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(listSlot));
            emit(out.code, OpCode::CallMethod, addString(module, "size"), 0);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(sizeSlot));

            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(sizeSlot));
            emit(out.code, OpCode::LessThan);
            const std::size_t exitJump = emitJump(out.code, OpCode::JumpIfFalse);

            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(listSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::CallMethod, addString(module, "get"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(itemSlot));

            LoopContext localLoop;
            compileStatements(stmt.body, module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, &localLoop);

            localLoop.continueTarget = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(1)));
            emit(out.code, OpCode::Add);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));
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
            const auto keySlot = ensureLocal(locals, out, stmt.iterKey);
            const auto valueSlot = ensureLocal(locals, out, stmt.iterValue);
            const auto dictSlot = ensureLocal(locals, out, "__for_dict_" + stmt.iterKey + std::to_string(out.code.size()));
            const auto indexSlot = ensureLocal(locals, out, "__for_idx_" + stmt.iterKey + std::to_string(out.code.size()));
            const auto sizeSlot = ensureLocal(locals, out, "__for_size_" + stmt.iterKey + std::to_string(out.code.size()));

            compileExpr(stmt.iterable, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(dictSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));

            const std::size_t loopStart = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(dictSlot));
            emit(out.code, OpCode::CallMethod, addString(module, "size"), 0);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(sizeSlot));

            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(sizeSlot));
            emit(out.code, OpCode::LessThan);
            const std::size_t exitJump = emitJump(out.code, OpCode::JumpIfFalse);

            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(dictSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::CallMethod, addString(module, "key_at"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(keySlot));

            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(dictSlot));
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::CallMethod, addString(module, "value_at"), 1);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(valueSlot));

            LoopContext localLoop;
            compileStatements(stmt.body, module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, &localLoop);

            localLoop.continueTarget = out.code.size();
            emit(out.code, OpCode::LoadLocal, static_cast<std::int32_t>(indexSlot));
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(1)));
            emit(out.code, OpCode::Add);
            emit(out.code, OpCode::StoreLocal, static_cast<std::int32_t>(indexSlot));
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
                compileExpr(stmt.branchConditions[i], module, locals, funcIndex, classIndex, currentFunctionName, out.code);
                const std::size_t falseJump = emitJump(out.code, OpCode::JumpIfFalse);
                compileStatements(stmt.branchBodies[i], module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, loopContext);
                endJumps.push_back(emitJump(out.code, OpCode::Jump));
                patchJump(out.code, falseJump, out.code.size());
            }

            if (!stmt.elseBody.empty()) {
                compileStatements(stmt.elseBody, module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, loopContext);
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

            compileExpr(stmt.condition, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            const std::size_t exitJump = emitJump(out.code, OpCode::JumpIfFalse);

            compileStatements(stmt.body, module, locals, funcIndex, classIndex, currentFunctionName, isModuleInit, out, &localLoop);
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
            compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
            emit(out.code, OpCode::Pop);
            break;
        case StmtType::Return:
            compileExpr(stmt.expr, module, locals, funcIndex, classIndex, currentFunctionName, out.code);
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
    Module module;
    std::unordered_map<std::string, std::size_t> funcIndex;
    std::unordered_map<std::string, std::size_t> classIndex;

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
        FunctionBytecode& out = module.functions[funcIndex.at(fn.name)];
        validateScopeLocalRules(fn.body, out.params, fn.name);
        std::unordered_map<std::string, std::size_t> locals;
        for (std::size_t i = 0; i < out.params.size(); ++i) {
            locals[out.params[i]] = i;
        }

        compileStatements(fn.body, module, locals, funcIndex, classIndex, fn.name, false, out, nullptr);

        if (out.code.empty() || out.code.back().op != OpCode::Return) {
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::Return);
        }
    }

    for (const auto& cls : program.classes) {
        for (const auto& method : cls.methods) {
            const std::string mangled = mangleMethodName(cls.name, method.name);
            FunctionBytecode& out = module.functions[funcIndex.at(mangled)];
            validateScopeLocalRules(method.body, out.params, cls.name + "::" + method.name);
            std::unordered_map<std::string, std::size_t> locals;
            for (std::size_t i = 0; i < out.params.size(); ++i) {
                locals[out.params[i]] = i;
            }

            compileStatements(method.body,
                              module,
                              locals,
                              funcIndex,
                              classIndex,
                              cls.name + "::" + method.name,
                              false,
                              out,
                              nullptr);

            if (out.code.empty() || out.code.back().op != OpCode::Return) {
                emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
                emit(out.code, OpCode::Return);
            }
        }
    }

    {
        FunctionBytecode& out = module.functions[funcIndex.at(moduleInitName)];
        validateScopeLocalRules(program.topLevelStatements, {}, moduleInitName);
        std::unordered_map<std::string, std::size_t> locals;
        compileStatements(program.topLevelStatements,
                          module,
                          locals,
                          funcIndex,
                          classIndex,
                          moduleInitName,
                          true,
                          out,
                          nullptr);
        if (out.code.empty() || out.code.back().op != OpCode::Return) {
            emit(out.code, OpCode::PushConst, addConstant(module, Value::Int(0)));
            emit(out.code, OpCode::Return);
        }
    }

    return module;
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
        return compileSource(mergedSource);
    } catch (const std::exception& ex) {
        throw std::runtime_error(path + ":" + tryFillFunctionContext(ex.what(), mergedSource));
    }
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
        out << fn.code.size() << "\n";
        for (const auto& ins : fn.code) {
            out << static_cast<int>(ins.op) << ' ' << ins.a << ' ' << ins.b << "\n";
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

        std::size_t codeCount = 0;
        in >> codeCount;
        fn.code.reserve(codeCount);
        for (std::size_t c = 0; c < codeCount; ++c) {
            int op = 0;
            Instruction ins;
            in >> op >> ins.a >> ins.b;
            ins.op = static_cast<OpCode>(op);
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
        for (const auto& ins : fn.code) {
            out << "        f.code.push_back(gs::Instruction{gs::OpCode::";
            switch (ins.op) {
            case OpCode::PushConst: out << "PushConst"; break;
            case OpCode::LoadLocal: out << "LoadLocal"; break;
            case OpCode::LoadName: out << "LoadName"; break;
            case OpCode::StoreName: out << "StoreName"; break;
            case OpCode::StoreLocal: out << "StoreLocal"; break;
            case OpCode::Add: out << "Add"; break;
            case OpCode::Sub: out << "Sub"; break;
            case OpCode::Mul: out << "Mul"; break;
            case OpCode::Div: out << "Div"; break;
            case OpCode::LessThan: out << "LessThan"; break;
            case OpCode::GreaterThan: out << "GreaterThan"; break;
            case OpCode::Equal: out << "Equal"; break;
            case OpCode::NotEqual: out << "NotEqual"; break;
            case OpCode::LessEqual: out << "LessEqual"; break;
            case OpCode::GreaterEqual: out << "GreaterEqual"; break;
            case OpCode::Jump: out << "Jump"; break;
            case OpCode::JumpIfFalse: out << "JumpIfFalse"; break;
            case OpCode::CallHost: out << "CallHost"; break;
            case OpCode::CallFunc: out << "CallFunc"; break;
            case OpCode::NewInstance: out << "NewInstance"; break;
            case OpCode::LoadAttr: out << "LoadAttr"; break;
            case OpCode::StoreAttr: out << "StoreAttr"; break;
            case OpCode::CallMethod: out << "CallMethod"; break;
            case OpCode::CallValue: out << "CallValue"; break;
            case OpCode::CallIntrinsic: out << "CallIntrinsic"; break;
            case OpCode::SpawnFunc: out << "SpawnFunc"; break;
            case OpCode::Await: out << "Await"; break;
            case OpCode::MakeList: out << "MakeList"; break;
            case OpCode::MakeDict: out << "MakeDict"; break;
            case OpCode::Sleep: out << "Sleep"; break;
            case OpCode::Yield: out << "Yield"; break;
            case OpCode::Return: out << "Return"; break;
            case OpCode::Pop: out << "Pop"; break;
            }
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
