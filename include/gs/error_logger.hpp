#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace gs {

/**
 * ErrorLogger - Writes detailed error information to Error.log
 * This helps AI tools and developers diagnose crashes.
 */
class ErrorLogger {
public:
    static ErrorLogger& instance() {
        static ErrorLogger inst;
        return inst;
    }

    // Log an error with context
    void logError(const std::string& errorMessage,
                  const std::string& functionName = "",
                  const std::string& fileName = "",
                  int lineNumber = 0);

    // Log an error with VM state
    void logVmError(const std::string& errorMessage,
                    const std::string& currentFunction,
                    std::size_t lineNumber,
                    const std::vector<std::string>& callStack,
                    const std::string& additionalContext = "");

    // Log a general exception
    void logException(const std::exception& ex,
                      const std::string& context = "");

    // Add custom context to the next error log
    void addContext(const std::string& key, const std::string& value);
    void clearContext();

    // Set log file path (default: Error.log in current directory)
    void setLogPath(const std::string& path);

private:
    ErrorLogger();
    ~ErrorLogger() = default;

    std::string getTimestamp() const;
    void writeToFile(const std::string& content);

    std::string logPath_ = "Error.log";
    std::vector<std::pair<std::string, std::string>> contextItems_;
};

// Helper macro for convenient error logging
#define GS_LOG_ERROR(msg) \
    gs::ErrorLogger::instance().logError(msg, __FUNCTION__, __FILE__, __LINE__)

#define GS_LOG_EXCEPTION(ex) \
    gs::ErrorLogger::instance().logException(ex, std::string(__FUNCTION__) + " at " + __FILE__ + ":" + std::to_string(__LINE__))

} // namespace gs
