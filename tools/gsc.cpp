#include "gs/compiler.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace {

bool writeAll(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: gsc <input.gs> <output.gsbc> [--aot-cpp <output.cpp>]\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    try {
        const gs::Module module = gs::compileSourceFile(inputPath);
        if (!writeAll(outputPath, gs::serializeModuleText(module))) {
            std::cerr << "Failed to write bytecode file: " << outputPath << "\n";
            return 3;
        }

        if (argc == 5 && std::string(argv[3]) == "--aot-cpp") {
            const std::string cppOut = argv[4];
            if (!writeAll(cppOut, gs::generateAotCpp(module, "build_script_module"))) {
                std::cerr << "Failed to write AOT C++ file: " << cppOut << "\n";
                return 4;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Compile failed: " << ex.what() << "\n";
        return 10;
    }

    std::cout << "OK\n";
    return 0;
}
