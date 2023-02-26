
#include "interpreter.hpp"
#include "jit.hpp"
#include "optimizer.hpp"
#include "parser.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/color.h>
#include <string_view>

std::string load_program(char const* path);
void print_usage(char const* argv);

int main(int const argc, char const *argv[]) {
    std::string program;
    char const* program_path = nullptr;
    bool run_interpreter = false;
    bool do_not_optimize = false;

    for (int i = 1; i < argc; i++) {
        auto arg = std::string_view{ argv[i] };
        if (arg.starts_with("-")) {
            if (arg == "-i") {
                run_interpreter = true;
            } else if (arg == "-d") {
                do_not_optimize = true;
            } else if (arg == "-h") {
                print_usage(argv[0]);
                return 0;
            } else {
                fmt::print("unknown flag: {}\n", arg);
                print_usage(argv[0]);
                return 1;
            }
        } else {
            program_path = argv[i];
        }
    }
    if (program_path == nullptr) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error");
        fmt::print(": file to run not specified\n");
        print_usage(argv[0]);
        return 1;
    }
    program = load_program(program_path);
    auto bytecode = bfjit::parse_program(program);
    if (!do_not_optimize)
        bytecode = bfjit::optimize(bytecode);

    if (run_interpreter) {
        auto interpreter = bfjit::Interpreter( bytecode );
        interpreter.run_until_end();
    } else {
        auto jit = bfjit::JIT( bytecode );
        jit.do_codegen();
        jit.run_until_end();
    }
}

std::string load_program(char const* path) {
    if (!std::filesystem::is_regular_file(path)) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error");
        fmt::print(": file is not a regular file\n");
        std::abort();
    }
    auto const fsize = std::filesystem::file_size(path);
    if (fsize > 1024*1024) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error");
        fmt::print(": file is too big ({} KB)\n", (fsize + 511) / 1024);
        std::abort();
    }
    auto handle = std::ifstream( path );
    std::string buffer;
    buffer.resize(fsize+1, 0);
    handle.read(buffer.data(), fsize);
    return buffer;
}


void print_usage(char const* argv) {
    fmt::print(R"(Usage:
{} [-d] [-i] SOURCE_FILE
OPTIONS:
    -d      disable optimizations
    -i      use interpreter instead of JIT
    -h      print this message
)", argv);
}