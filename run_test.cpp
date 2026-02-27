#include "gs/runtime.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_test <script.gs>" << std::endl;
        return 1;
    }

    try {
        gs::Runtime runtime;
        
        // Load script from current directory
        std::string scriptName = argv[1];
        
        // Check if it's a bytecode file or source file
        bool success = false;
        if (scriptName.ends_with(".gsbc")) {
            success = runtime.loadBytecodeFile(scriptName);
        } else {
            std::vector<std::string> searchPaths = {"."};
            success = runtime.loadSourceFile(scriptName, searchPaths);
        }
        
        if (!success) {
            std::cerr << "Failed to load script: " << scriptName << std::endl;
            if (!runtime.lastError().empty()) {
                std::cerr << "Error: " << runtime.lastError() << std::endl;
            }
            return 1;
        }
        
        // Execute main function
        const auto result = runtime.call("main");
        std::cout << "\nmain() returned: " << result << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    }
}