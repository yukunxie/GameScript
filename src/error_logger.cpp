#include "gs/error_logger.hpp"

#include <iomanip>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace gs {

ErrorLogger::ErrorLogger() {
    // Clear the log file on startup (optional - you can remove this to append)
    // std::ofstream ofs(logPath_, std::ios::trunc);
}

std::string ErrorLogger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void ErrorLogger::writeToFile(const std::string& content) {
    std::ofstream ofs(logPath_, std::ios::app);
    if (ofs.is_open()) {
        ofs << content;
        ofs.flush();
    }
    // Also output to stderr for immediate feedback
    std::cerr << content;
}

void ErrorLogger::logError(const std::string& errorMessage,
                           const std::string& functionName,
                           const std::string& fileName,
                           int lineNumber) {
    std::ostringstream oss;
    oss << "\n";
    oss << "================================================================================\n";
    oss << "ERROR LOG - " << getTimestamp() << "\n";
    oss << "================================================================================\n";
    oss << "Message: " << errorMessage << "\n";
    
    if (!functionName.empty()) {
        oss << "Function: " << functionName << "\n";
    }
    if (!fileName.empty()) {
        oss << "File: " << fileName << "\n";
    }
    if (lineNumber > 0) {
        oss << "Line: " << lineNumber << "\n";
    }
    
    // Add custom context
    if (!contextItems_.empty()) {
        oss << "\n--- Context ---\n";
        for (const auto& item : contextItems_) {
            oss << item.first << ": " << item.second << "\n";
        }
    }
    
#ifdef _WIN32
    // Capture stack trace on Windows
    oss << "\n--- Stack Trace ---\n";
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);
    
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    if (symbol) {
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        
        for (WORD i = 0; i < frames; i++) {
            if (SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol)) {
                oss << "  [" << i << "] " << symbol->Name << " (0x" 
                    << std::hex << symbol->Address << std::dec << ")\n";
            } else {
                oss << "  [" << i << "] <unknown> (0x" 
                    << std::hex << (DWORD64)stack[i] << std::dec << ")\n";
            }
        }
        free(symbol);
    }
#endif
    
    oss << "================================================================================\n\n";
    
    writeToFile(oss.str());
    clearContext();
}

void ErrorLogger::logVmError(const std::string& errorMessage,
                             const std::string& currentFunction,
                             std::size_t lineNumber,
                             const std::vector<std::string>& callStack,
                             const std::string& additionalContext) {
    std::ostringstream oss;
    oss << "\n";
    oss << "================================================================================\n";
    oss << "VM ERROR LOG - " << getTimestamp() << "\n";
    oss << "================================================================================\n";
    oss << "Message: " << errorMessage << "\n";
    oss << "\n--- VM State ---\n";
    oss << "Current Function: " << currentFunction << "\n";
    if (lineNumber > 0) {
        oss << "Source Line: " << lineNumber << "\n";
    } else {
        oss << "Source Line: <unknown>\n";
    }
    
    if (!callStack.empty()) {
        oss << "\n--- Script Call Stack ---\n";
        for (std::size_t i = 0; i < callStack.size(); ++i) {
            oss << "  [" << i << "] " << callStack[i] << "\n";
        }
    }
    
    if (!additionalContext.empty()) {
        oss << "\n--- Additional Context ---\n";
        oss << additionalContext << "\n";
    }
    
    // Add custom context
    if (!contextItems_.empty()) {
        oss << "\n--- Debug Context ---\n";
        for (const auto& item : contextItems_) {
            oss << item.first << ": " << item.second << "\n";
        }
    }
    
#ifdef _WIN32
    // Capture stack trace on Windows
    oss << "\n--- Native Stack Trace ---\n";
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    
    // Initialize symbol handler with better options
    DWORD symOptions = SymGetOptions();
    symOptions |= SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES;
    SymSetOptions(symOptions);
    
    if (!SymInitialize(process, NULL, TRUE)) {
        oss << "  Failed to initialize symbol handler (error: " << GetLastError() << ")\n";
    } else {
        WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);
        
        // Allocate symbol info buffer with larger name buffer
        const size_t symbolBufferSize = sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR);
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(symbolBufferSize);
        IMAGEHLP_LINE64 line = { 0 };
        IMAGEHLP_MODULE64 module = { 0 };
        
        if (symbol) {
            symbol->MaxNameLen = MAX_SYM_NAME;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            
            for (WORD i = 0; i < frames; i++) {
                DWORD64 address = (DWORD64)stack[i];
                
                // Try to get symbol name
                DWORD64 displacement = 0;
                bool hasSymbol = SymFromAddr(process, address, &displacement, symbol);
                
                // Try to get module name
                bool hasModule = SymGetModuleInfo64(process, address, &module);
                
                // Try to get file and line info
                DWORD lineDisplacement = 0;
                bool hasLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &line);
                
                oss << "  [" << std::setw(2) << i << "] ";
                
                // Module name
                if (hasModule) {
                    oss << module.ModuleName << "!";
                }
                
                // Symbol name
                if (hasSymbol) {
                    oss << symbol->Name;
                    if (displacement > 0) {
                        oss << "+0x" << std::hex << displacement << std::dec;
                    }
                } else {
                    oss << "<unknown>";
                }
                
                // Address
                oss << " (0x" << std::hex << address << std::dec << ")";
                
                // File and line info
                if (hasLine) {
                    oss << " [" << line.FileName << ":" << line.LineNumber << "]";
                }
                
                oss << "\n";
            }
            free(symbol);
        } else {
            oss << "  Failed to allocate symbol buffer\n";
        }
        
        SymCleanup(process);
    }
#endif
    
    oss << "================================================================================\n\n";
    
    writeToFile(oss.str());
    clearContext();
}

void ErrorLogger::logException(const std::exception& ex,
                               const std::string& context) {
    std::ostringstream oss;
    oss << "\n";
    oss << "================================================================================\n";
    oss << "EXCEPTION LOG - " << getTimestamp() << "\n";
    oss << "================================================================================\n";
    oss << "Exception Type: " << typeid(ex).name() << "\n";
    oss << "Message: " << ex.what() << "\n";
    
    if (!context.empty()) {
        oss << "Context: " << context << "\n";
    }
    
    // Add custom context
    if (!contextItems_.empty()) {
        oss << "\n--- Debug Context ---\n";
        for (const auto& item : contextItems_) {
            oss << item.first << ": " << item.second << "\n";
        }
    }
    
#ifdef _WIN32
    // Capture stack trace on Windows
    oss << "\n--- Stack Trace ---\n";
    void* stack[64];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);
    
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    if (symbol) {
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        
        for (WORD i = 0; i < frames; i++) {
            if (SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol)) {
                oss << "  [" << i << "] " << symbol->Name << " (0x" 
                    << std::hex << symbol->Address << std::dec << ")\n";
            } else {
                oss << "  [" << i << "] <unknown> (0x" 
                    << std::hex << (DWORD64)stack[i] << std::dec << ")\n";
            }
        }
        free(symbol);
    }
#endif
    
    oss << "================================================================================\n\n";
    
    writeToFile(oss.str());
    clearContext();
}

void ErrorLogger::addContext(const std::string& key, const std::string& value) {
    contextItems_.emplace_back(key, value);
}

void ErrorLogger::clearContext() {
    contextItems_.clear();
}

void ErrorLogger::setLogPath(const std::string& path) {
    logPath_ = path;
}

} // namespace gs
