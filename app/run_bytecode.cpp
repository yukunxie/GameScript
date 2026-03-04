#include "gs/runtime.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_bytecode <file.gsbc>" << std::endl;
        return 1;
    }

    try {
        gs::Runtime runtime;
        const std::string path = argv[1];

        if (!path.ends_with(".gsbc")) {
            std::cerr << "run_bytecode expects a .gsbc file: " << path << std::endl;
            return 1;
        }

        if (!runtime.loadBytecodeFile(path)) {
            std::cerr << "Failed to load bytecode: " << path << std::endl;
            if (!runtime.lastError().empty()) {
                std::cerr << "Error: " << runtime.lastError() << std::endl;
            }
            return 1;
        }

        const auto result = runtime.call("main");
        std::cout << "main() returned: " << result << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
