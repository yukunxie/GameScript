#include "gs/runtime.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_test <script.gs|script.gsbc>" << std::endl;
        return 1;
    }

    try {
        gs::Runtime runtime;

        const std::string scriptName = argv[1];
        bool success = false;
        if (scriptName.ends_with(".gsbc")) {
            success = runtime.loadBytecodeFile(scriptName);
        } else {
            std::vector<std::string> searchPaths = {
                ".",
                "scripts",
                "../scripts",
                "../../scripts",
                "../../../scripts"
            };
            success = runtime.loadSourceFile(scriptName, searchPaths);
        }

        if (!success) {
            std::cerr << "Failed to load script: " << scriptName << std::endl;
            if (!runtime.lastError().empty()) {
                std::cerr << "Error: " << runtime.lastError() << std::endl;
            }
            return 1;
        }

        const auto result = runtime.call("main");
        std::cout << "\nmain() returned: " << result << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    }
}
