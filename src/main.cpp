
#include "interpreter.hpp"
#include "jit.hpp"
#include "optimizer.hpp"
#include "options.hpp"
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
size_t print_bfcode(std::vector<bfjit::BFOp> const& code, size_t start = 0, size_t offset = 0);

int main(int const argc, char const *argv[]) {
    std::string program;
    char const* program_path = nullptr;
    bool run_interpreter = false;
    bool do_not_optimize = false;
    bool print_and_exit = false;
    bfjit::CLIOpts cli_opts;

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
            } else if (arg == "-p") {
                print_and_exit = true;
            } else if (arg == "-v") {
                cli_opts.debug_info = true;
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

    if (print_and_exit) {
        print_bfcode(bytecode);
        return 0;
    }

    if (run_interpreter) {
        auto interpreter = bfjit::Interpreter( bytecode );
        interpreter.run_until_end();
    } else {
        auto jit = bfjit::JIT( bytecode, cli_opts );
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

size_t print_bfcode(std::vector<bfjit::BFOp> const& code, size_t start, size_t offset) {
    size_t i = start;
    for (; i < code.size(); i++) {
        auto const& bc = code[i];
        if (bc.m_type != bfjit::BFOp::Type::LoopEnd)
            for (int j = 0; j < offset; j++) fmt::print(" ");
        switch (bc.m_type) {
        case bfjit::BFOp::Type::Mod:
            fmt::print("<{}:{}>\n", bc.inc_arg < 0 ? '-' : '+', int8_t(bc.inc_arg));
            break;
        case bfjit::BFOp::Type::ModPtr:
            fmt::print("<{}:{}>\n", bc.inc_ptr_arg < 0 ? '<' : '>', bc.inc_ptr_arg);
            break;
        case bfjit::BFOp::Type::In:
            fmt::print("<In>\n");
            break;
        case bfjit::BFOp::Type::Out:
            fmt::print("<Out>\n");
            break;
        case bfjit::BFOp::Type::LoopBeg:
            fmt::print("<LoopBegin>\n");
            i = print_bfcode(code, i+1, offset+1);
            for (int j = 0; j < offset; j++) fmt::print(" ");
            fmt::print("<LoopEnd>\n");
            break;
        case bfjit::BFOp::Type::LoopEnd:
            return i;
        case bfjit::BFOp::Type::SetValue:
            fmt::print("<Set:{}>\n", bc.set_arg);
            break;
        case bfjit::BFOp::Type::Halt:
            fmt::print("<Halt>\n");
          break;
        }
    }
    return i;
}

void print_usage(char const* argv) {
    fmt::print(R"(Usage:
{} [-d] [-i] SOURCE_FILE
OPTIONS:
    -d      disable optimizations
    -i      use interpreter instead of JIT
    -h      print this message
    -p      print bytecode before execution and exit
)", argv);
}
