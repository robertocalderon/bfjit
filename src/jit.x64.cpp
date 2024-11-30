
#include "jit.hpp"
#include "asmjit/core/operand.h"
#include "options.hpp"
#include "parser.hpp"

#include <cstdlib>
#include <fmt/format.h>

#include <stack>

namespace x64 = asmjit::x86;
constexpr auto DATA_BASE   = x64::rcx;
constexpr auto DATA_INDEX  = x64::rdx;
constexpr auto CACHE_VALUE = x64::r8b;

struct EHandler : public asmjit::ErrorHandler {
	void handleError(asmjit::Error err, char const* msg, asmjit::BaseEmitter*) override {
		fmt::print("asmjit error: {} ({})", msg, err);
		std::abort();
	}
};

#ifdef _WIN32
void __fastcall print_char(uint64_t r) {
#else
void print_char(uint64_t r) {
#endif
	r = r & 0xff;
	fmt::print("{}", char(r));
}
#ifdef _WIN32
void __fastcall print_char(uint64_t r) {
#else
void outsize_of_bounds() {
#endif
	fmt::print("trying to access data outside of bouds\n");
}

void do_codegen(asmjit::x86::Assembler& a, std::span<bfjit::BFOp const> code, asmjit::Label& exit, asmjit::Label& outside_bounds, uint32_t data_size, std::vector<size_t>& jump_offsets);

namespace bfjit {

    struct JIT::InnerData {};

    JIT::JIT(std::span<BFOp const> bytecode, bfjit::CLIOpts const& cli_opts) :
        m_ip(0),
        m_ptr(0),
        m_bytecode(bytecode),
	m_cli_opts(cli_opts)
    {
        m_buffer.resize(4096);
    }
    JIT::~JIT() = default;

    void JIT::do_codegen() {
        size_t ip = 0;

        EHandler ehandler;

        asmjit::CodeHolder code_holder;
        code_holder.init(runtime.environment());
        code_holder.setErrorHandler(&ehandler);

        asmjit::x86::Assembler a(&code_holder);
#ifdef _WIN32
        #error "not implemented yet for windows"
        //a.mov(x64::rcx, (uint64_t)data.data());
        //a.mov(x64::rdx, 0);
        a.mov(x64::r9, x64::r8);
        a.mov(x64::r8, 0);
        a.jmp(x64::r9);
#else
        a.mov(x64::r9, x64::rdx);
        a.mov(DATA_BASE, x64::rdi);
        a.mov(DATA_INDEX, x64::rsi);
        a.mov(CACHE_VALUE, x64::ptr( DATA_BASE, DATA_INDEX ));
        a.jmp(x64::r9);
#endif
        auto exit_label = a.newLabel();
		auto outside_of_bounds = a.newLabel();

        ::do_codegen(a, m_bytecode, exit_label, outside_of_bounds, (uint32_t)this->m_buffer.size(), this->mapping_bytecode_to_code);

        a.bind(exit_label);
        a.ret();
        a.bind(outside_of_bounds);

		a.push(x64::rbp);
		a.mov(x64::rbp, x64::rsp);
		a.add(x64::rsp, 15);
		a.and_(x64::rsp, uint64_t(~0xf));
        a.call(outside_of_bounds);
        a.mov( x64::rsp, x64::rbp );
		a.pop( x64::rbp );

        a.ret();

        //void(__fastcall* func)(uint64_t base, uint64_t idx, uint64_t addr);
        MFuncType func;
        auto const err = runtime.add(&func, &code_holder);
        if (err) {
            fmt::print("error creating the function, err: {}", err);
            return;
        }
        this->main_function = func;
    }
    void JIT::run_until_end() {
        if (this->main_function) {
            // skip if already completed
            if (m_ip >= mapping_bytecode_to_code.size())
                return;

            auto data = uint64_t(this->m_buffer.data());
            auto const offset = mapping_bytecode_to_code[m_ip];
            auto const addr = uint64_t(this->main_function) + offset;
            this->main_function(data, this->m_ptr, addr, nullptr);
        }
        else {
            fmt::print("you need to call do_codegen first\n");
        }
    }
}

void do_codegen(asmjit::x86::Assembler& a, std::span<bfjit::BFOp const> code, asmjit::Label& exit, asmjit::Label& outside_bounds, uint32_t data_size, std::vector<size_t>& jump_offsets) {
    std::stack<asmjit::Label> loop_labels;
	for (size_t i = 0; i < code.size(); i++) {
		auto const& op = code[i];
		switch (op.m_type) {
		case bfjit::BFOp::Type::Mod:
			jump_offsets.push_back(a.offset());
			a.add(CACHE_VALUE, uint8_t(op.inc_arg));
			break;
		case bfjit::BFOp::Type::ModPtr:
			jump_offsets.push_back(a.offset());
			// Save cached data
			a.mov(x64::ptr(DATA_BASE, DATA_INDEX), CACHE_VALUE);
			// Increment index
			a.add(DATA_INDEX, int32_t(op.inc_ptr_arg));
			// Check if next step will get out of bounds
			a.cmp(DATA_INDEX, data_size - 1);
			a.ja(outside_bounds);
			// Load new data
			a.mov(CACHE_VALUE, x64::ptr(DATA_BASE, DATA_INDEX));
			break;
		case bfjit::BFOp::Type::Out:
			jump_offsets.push_back(a.offset());
#ifdef _WIN32
			a.push(DATA_BASE);
			a.push(DATA_INDEX);
			a.push(x64::r8);
			a.mov(x64::rcx, x64::r8);

			a.call(print_char);

			a.pop(x64::r8);
			a.pop(DATA_INDEX);
			a.pop(DATA_BASE);
			break;
#else
			a.push(DATA_BASE);
			a.push(DATA_INDEX);
			a.push(x64::r8);
			a.mov(x64::rdi, x64::r8);

			// align stack
			a.push(x64::rbp);
			a.mov(x64::rbp, x64::rsp);
			a.sub(x64::rsp, 15);
			a.and_(x64::rsp, uint64_t(~0xf));

			a.call(print_char);

			a.mov( x64::rsp, x64::rbp );
			a.pop( x64::rbp );

			a.pop(x64::r8);
			a.pop(DATA_INDEX);
			a.pop(DATA_BASE);
			break;
#endif
		case bfjit::BFOp::Type::LoopBeg: {
			jump_offsets.push_back(a.offset());
			auto end = a.newLabel();
			auto start = a.newLabel();
			a.bind(start);
			a.cmp(CACHE_VALUE, 0);
			a.je(end);

            loop_labels.push( start );
            loop_labels.push( end );

			break;
		}
		case bfjit::BFOp::Type::LoopEnd:{
            auto end = loop_labels.top();
            loop_labels.pop();
            auto start = loop_labels.top();
            loop_labels.pop();
            jump_offsets.push_back(a.offset());
			a.jmp(start);
			a.bind(end);
        }
			break;
		case bfjit::BFOp::Type::SetValue:
			jump_offsets.push_back(a.offset());
            if (op.inc_arg == 0)
                a.xor_(x64::r8, x64::r8);
            else
                a.mov( x64::r8, uint8_t( op.inc_arg ) );
			break;
        case bfjit::BFOp::Type::In:
            std::abort();
        case bfjit::BFOp::Type::Halt:
            std::abort();
		}
	}
}
