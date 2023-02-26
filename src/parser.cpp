
#include "parser.hpp"
#include "fmt/core.h"
#include <algorithm>
#include <iterator>
#include <stack>
#include <fmt/format.h>
#include <fmt/color.h>

namespace bfjit {
    [[noreturn]]
    void panic_not_opened_loop();

    auto skip_comment(std::string_view program) -> std::string_view {
        if (program.starts_with("[")) {
            uint64_t cnt = 1;
            program = program.substr(1);
            while (cnt > 0 && !program.empty()) {
                auto const ch = program[0];
                if (ch == '[')
                    cnt++;
                else if (ch == ']')
                    cnt--;
                
                program = program.substr(1);
            }
            if (cnt != 0) {
                fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error");
                fmt::print(": program stars with a comment but ends before the comment\n");
            }
        }
        return program;
    }
    auto parse_program(std::string_view program) -> std::vector<BFOp> {
        auto ret = std::vector<BFOp>();
        ret.reserve(std::min<size_t>(program.size(), 1024 * 1024));
        
        program = skip_comment(program);
        std::stack<size_t> loop_stack;

        for (auto const& ch : program) {
            auto const c_pos = ret.size();

            switch (ch) {
                case '+': ret.push_back( BFOp{ .m_type = BFOp::Type::Mod, .inc_arg = 1   } ); break;
                case '-': ret.push_back( BFOp{ .m_type = BFOp::Type::Mod, .inc_arg = 255 } ); break;
                case '<': ret.push_back( BFOp{ .m_type = BFOp::Type::ModPtr, .inc_ptr_arg = -1 } ); break;
                case '>': ret.push_back( BFOp{ .m_type = BFOp::Type::ModPtr, .inc_ptr_arg =  1 } ); break;
                case '.': ret.push_back( BFOp{ .m_type = BFOp::Type::Out } ); break;
                case ',': ret.push_back( BFOp{ .m_type = BFOp::Type::In } ); break;
                case '[':
                    loop_stack.push( c_pos );
                    ret.push_back( BFOp{ .m_type = BFOp::Type::LoopBeg, .loop_arg = c_pos } );
                    break;
                case ']':
                    if (loop_stack.empty()) {
                        panic_not_opened_loop();
                    } else {
                        auto loop_beg = loop_stack.top();
                        loop_stack.pop();
                        ret.push_back( BFOp{ .m_type = BFOp::Type::LoopEnd, .loop_arg = loop_beg } );
                        ret[loop_beg].loop_arg = c_pos;
                    }
                    break;
                default: break;
            }
        }

        return ret;
    }

    void panic_not_opened_loop() {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "error");
        fmt::print(": program contains a loop ending operator (\"]\") that has no corresponding loop begining operator (\"[\")\n");
        std::abort();
    }
}
