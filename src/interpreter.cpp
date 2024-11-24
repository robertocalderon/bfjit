
#include "interpreter.hpp"
#include "parser.hpp"
#include <cstdlib>
#include <cstdint>
#include <fmt/format.h>
#include <span>

namespace bfjit {

    Interpreter::Interpreter(std::span<BFOp const> bytecode) :
        m_ip(0),
        m_ptr(0),
        m_bytecode(bytecode)
    {
        m_buffer.resize(4096);
    }

    auto Interpreter::finished() const -> bool {
        return m_ip == m_bytecode.size();
    }
    auto Interpreter::run_one_step() -> bool {
        if (finished())
            return false;

        auto c_inst = m_bytecode[m_ip++];
        switch (c_inst.m_type) {
            case BFOp::Type::Mod:
                m_buffer[m_ptr] += c_inst.inc_arg;
                break;
            case BFOp::Type::ModPtr:
                {
                    auto const new_ptr = int64_t(m_ptr) + c_inst.inc_ptr_arg;
                    if (new_ptr < 0 || new_ptr >= m_buffer.size()) {
                        std::abort();
                    }
                    m_ptr = new_ptr;
                }
                break;
            case BFOp::Type::In:
                std::abort();
            case BFOp::Type::Out:
                fmt::print("{}", char(m_buffer[m_ptr]));
                break;
            case BFOp::Type::LoopBeg:
                if (m_buffer[m_ptr] == 0) {
                    m_ip = c_inst.loop_arg+1;
                }
                break;
            case BFOp::Type::LoopEnd:
                if (m_buffer[m_ptr] != 0) {
                    m_ip = c_inst.loop_arg+1;
                }
                break;
            case BFOp::Type::SetValue:
                m_buffer[m_ptr] = c_inst.set_arg;
                break;
            case BFOp::Type::Halt:
                switch (c_inst.halt_reason) {
                    case BFOp::HaltReason::InfiniteLoop:
                        if (m_buffer[m_ip] == 0)
                            return true;
                        fmt::print("halted, reason: infinte loop reached\n");
                        break;
                    default:
                        fmt::print("halted, reason: unknown\n");
                }
                return false;
            default: std::abort();
        }

        return true;
    }
    void Interpreter::run_until_end() {
        while (this->run_one_step());
    }
}
