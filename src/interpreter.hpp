#pragma once

#include "parser.hpp"
#include <cstdint>
#include <vector>
#include <span>

namespace bfjit {

    struct Interpreter {
        std::vector<uint8_t> m_buffer;
        size_t m_ptr;
        size_t m_ip;
        std::span<BFOp const> m_bytecode;

        Interpreter(std::span<BFOp const> bytecode);
        ~Interpreter() = default;
        Interpreter(Interpreter const&) = default;
        Interpreter(Interpreter &&) = default;
        Interpreter& operator = (Interpreter const&) = default;
        Interpreter& operator = (Interpreter &&) = default;

        void run_until_end();

        [[nodiscard]]
        auto run_one_step() -> bool;
        [[nodiscard]]
        auto finished() const -> bool;
    };

}

