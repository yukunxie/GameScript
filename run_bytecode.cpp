#include "gs/vm.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: run_bytecode <file.gsbc>" << std::endl;
        return 1;
    }

    try {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file: " << argv[1] << std::endl;
            return 1;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string bytecode = buffer.str();

        gs::VirtualMachine vm;
        vm.run(bytecode);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
