
#include "optimizer.hpp"
#include "parser.hpp"
#include <iterator>
#include <optional>
#include <algorithm>
#include <numeric>
#include <stack>

namespace bfjit {
    auto match_manny(std::span<BFOp const> code, BFOp::Type type) -> size_t;
    bool matches(std::span<BFOp const> code, std::initializer_list<BFOp::Type> sequence);

    auto one_step_optimize(std::span<BFOp const> buffer_in, bool *did_something) -> std::vector<BFOp> {
        auto worked = [&]() { if (did_something) *did_something = true; };
        
        std::vector<BFOp> buffer;
        buffer.reserve(buffer_in.size());
        while (not buffer_in.empty()) {
            if (auto rep = match_manny(buffer_in, BFOp::Type::Mod); rep > 1) {
                uint8_t acc = 0;
                for (size_t i = 0; i < rep; i++)
                    acc += buffer_in[i].inc_arg;
                buffer_in = buffer_in.subspan(rep);
                buffer.push_back( BFOp{ .m_type = BFOp::Type::Mod, .inc_arg = acc } );
                worked();
                continue;
            }
            if (auto rep = match_manny(buffer_in, BFOp::Type::ModPtr); rep > 1) {
                int64_t acc = 0;
                for (size_t i = 0; i < rep; i++)
                    acc += buffer_in[i].inc_ptr_arg;
                buffer_in = buffer_in.subspan(rep);
                buffer.push_back( BFOp{ .m_type = BFOp::Type::ModPtr, .inc_ptr_arg = acc } );
                worked();
                continue;
            }

            if (matches(buffer_in, { BFOp::Type::LoopBeg, BFOp::Type::Mod, BFOp::Type::LoopEnd })) {
                if (buffer_in[1].inc_arg == 255) {
                    buffer_in = buffer_in.subspan<3>();
                    buffer.push_back( BFOp{ .m_type = BFOp::Type::SetValue, .set_arg = 0 } );
                    worked();
                    continue;
                }
            }
            if (matches(buffer_in, { BFOp::Type::LoopBeg, BFOp::Type::LoopEnd })) {
                buffer_in = buffer_in.subspan<2>();
                buffer.push_back( BFOp{ .m_type = BFOp::Type::Halt, .halt_reason = BFOp::HaltReason::InfiniteLoop } );
                worked();
                continue;
            }

            // TODO: loop reductions like [->+<] to add+setvalue or similar

            buffer.push_back(buffer_in[0]);
            buffer_in = buffer_in.subspan<1>();
        }
        return buffer;
    }
    auto optimize(std::span<BFOp> buffer_in) -> std::vector<BFOp> {
        bool did_something;
        std::vector<BFOp> buffer;
        do {
            did_something = false;
            buffer = one_step_optimize(buffer_in, &did_something);
            buffer_in = buffer;
        } while (did_something);
        do_loop_relink(buffer);
        return buffer;
    }
    void do_loop_relink(std::span<BFOp> buffer) {
        std::stack<size_t> loop_stack;
        for (size_t i = 0; i < buffer.size(); i++) {
            if (buffer[i].m_type == BFOp::Type::LoopBeg)
                loop_stack.push( i );
            else if (buffer[i].m_type == BFOp::Type::LoopEnd) {
                if (loop_stack.empty()) {
                    std::abort();
                } else {
                    auto loop_beg = loop_stack.top();
                    loop_stack.pop();
                    buffer[loop_beg].loop_arg = i;
                    buffer[i].loop_arg = loop_beg;
                }
            }
        }
    }


    auto match_manny(std::span<BFOp const> code, BFOp::Type type) -> size_t {
        size_t ret = 0;
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].m_type != type)
                return ret;
            else
                ret++;
        }
        return ret;
    }
    bool matches(std::span<BFOp const> code, std::initializer_list<BFOp::Type> sequence) {
        if (code.size() < sequence.size())
            return false;
        for (size_t i = 0; i < sequence.size(); i++)
            if (code[i].m_type != (&*sequence.begin())[i])
                return false;
        return true;
    }
    size_t find_closing_loop(std::span<BFOp const> buffer_in) {
        size_t ret = 0;
        size_t cnt = 0;
        do {
            if (buffer_in[ret].m_type == BFOp::Type::LoopBeg)
                cnt++;
            else if (buffer_in[ret].m_type == BFOp::Type::LoopEnd)
                cnt--;
            ret++;
        } while (cnt != 0);

        return ret;
    }
    
}