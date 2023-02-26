#pragma once

#include <vector>
#include <cstdint>
#include <span>

#include "parser.hpp"

#include <asmjit/asmjit.h>

namespace bfjit {

    class JIT {
    public:
        std::vector<uint8_t> m_buffer;
        size_t m_ptr;
        size_t m_ip;
        std::span<BFOp const> m_bytecode;
        std::vector<size_t> mapping_bytecode_to_code;
        asmjit::JitRuntime runtime;
#ifdef _WIN32
	    using MFuncType = void(__fastcall*)(uint64_t base, uint64_t idx, uint64_t jump_addr);
#else 
	    using MFuncType = void(*)(uint64_t base, uint64_t idx, uint64_t jump_addr);
#endif
	    MFuncType main_function;

        JIT(std::span<BFOp const> bytecode);
        ~JIT() = default;
        JIT(JIT const&) = delete;
        JIT(JIT &&) = delete;
        JIT& operator = (JIT const&) = delete;
        JIT& operator = (JIT &&) = delete;

        void run_until_end();
        void do_codegen();
    };

}
