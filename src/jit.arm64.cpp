#include "jit.hpp"
#include "asmjit/a64.h"
#include "asmjit/core/operand.h"
#include "parser.hpp"

#include <cstdlib>
#include <fmt/format.h>

#include <stack>

namespace a64 = asmjit::a64;
constexpr auto DATA_BASE = a64::x7;
constexpr auto DATA_INDEX = a64::x6;
constexpr auto CACHE_VALUE = a64::x5;
constexpr auto CACHE_VALUE_W = a64::w5;

struct EHandler : public asmjit::ErrorHandler {
  void handleError(asmjit::Error err, char const *msg,
                   asmjit::BaseEmitter *) override {
    fmt::print("asmjit error: {} ({})\n", msg, err);
    std::abort();
  }
};

void print_char(uint32_t r) {
  r = r & 0xff;
  fmt::print("{}", char(r));
}

void outsize_of_bounds() {
  fmt::print("trying to access data outside of bouds\n");
}

void do_codegen(asmjit::a64::Assembler &a, std::span<bfjit::BFOp const> code,
                asmjit::Label &exit, asmjit::Label &outside_bounds,
                uint32_t data_size, std::vector<size_t> &jump_offsets);

namespace bfjit {
JIT::JIT(std::span<BFOp const> bytecode)
    : m_ip(0), m_ptr(0), m_bytecode(bytecode) {
  m_buffer.resize(4096);
}

void JIT::do_codegen() {
  size_t ip = 0;

  EHandler ehandler;

  asmjit::CodeHolder code_holder;
  code_holder.init(runtime.environment());
  code_holder.setErrorHandler(&ehandler);

  asmjit::a64::Assembler a(&code_holder);

  auto exit_label = a.newLabel();
  auto outside_of_bounds = a.newLabel();

  a.mov(DATA_BASE, a64::x0);
  a.mov(DATA_INDEX, a64::x1);
  a.mov(CACHE_VALUE, asmjit::Imm(0));

  a.sub(a64::sp, a64::sp, asmjit::Imm(16));
  a.str(a64::x29, a64::Mem(a64::sp, 0));
  a.str(a64::x30, a64::Mem(a64::sp, 8));
  a.mov(a64::x29, a64::sp);

  // a.br(a64::x2);

  ::do_codegen(a, m_bytecode, exit_label, outside_of_bounds,
               (uint32_t)this->m_buffer.size(), this->mapping_bytecode_to_code);

  a.bind(outside_of_bounds);

  a.bind(exit_label);
  a.mov(a64::sp, a64::x29);
  a.ldr(a64::x29, a64::Mem(a64::sp, 0));
  a.ldr(a64::x30, a64::Mem(a64::sp, 8));
  a.add(a64::sp, a64::sp, asmjit::Imm(16));
  a.ret(a64::x30);

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
    this->main_function(data, this->m_ptr, addr);
  } else {
    fmt::print("you need to call do_codegen first\n");
  }
}
} // namespace bfjit

void do_codegen(asmjit::a64::Assembler &a, std::span<bfjit::BFOp const> code,
                asmjit::Label &exit, asmjit::Label &outside_bounds,
                uint32_t data_size, std::vector<size_t> &jump_offsets) {
  std::stack<asmjit::Label> loop_labels;
  for (size_t i = 0; i < code.size(); i++) {
    auto const &op = code[i];
    switch (op.m_type) {
    case bfjit::BFOp::Type::Mod:
      jump_offsets.push_back(a.offset());
      a.add(CACHE_VALUE, CACHE_VALUE, asmjit::Imm(op.inc_arg));
      a.and_(CACHE_VALUE, CACHE_VALUE, asmjit::Imm(255));
      break;
    case bfjit::BFOp::Type::ModPtr:
      jump_offsets.push_back(a.offset());
      a.strb(CACHE_VALUE_W, asmjit::a64::Mem(DATA_BASE, DATA_INDEX));
      if (op.inc_ptr_arg < 0)
        a.sub(DATA_INDEX, DATA_INDEX, asmjit::Imm(-op.inc_ptr_arg));
      else
        a.add(DATA_INDEX, DATA_INDEX, asmjit::Imm(op.inc_ptr_arg));
      a.cmp(DATA_INDEX, asmjit::Imm(data_size));
      a.b_gt(outside_bounds);
      a.ldrb(CACHE_VALUE_W, asmjit::a64::Mem(DATA_BASE, DATA_INDEX));
      break;
    case bfjit::BFOp::Type::Out:
      jump_offsets.push_back(a.offset());
      a.sub(a64::sp, a64::sp, asmjit::Imm(32));
      a.str(CACHE_VALUE, a64::Mem(a64::sp, 0));
      a.str(DATA_INDEX, a64::Mem(a64::sp, 8));
      a.str(DATA_BASE, a64::Mem(a64::sp, 16));

      a.mov(a64::x0, CACHE_VALUE);
      a.bl(asmjit::Imm(print_char));

      a.ldr(CACHE_VALUE, a64::Mem(a64::sp, 0));
      a.ldr(DATA_INDEX, a64::Mem(a64::sp, 8));
      a.ldr(DATA_BASE, a64::Mem(a64::sp, 16));
      a.add(a64::sp, a64::sp, asmjit::Imm(32));
      break;
    case bfjit::BFOp::Type::LoopBeg: {
      jump_offsets.push_back(a.offset());
      auto end = a.newLabel();
      auto start = a.newLabel();
      a.bind(start);
      a.cmp(CACHE_VALUE, 0);
      a.b_eq(end);

      loop_labels.push(start);
      loop_labels.push(end);
      break;
    }
    case bfjit::BFOp::Type::LoopEnd: {
      auto end = loop_labels.top();
      loop_labels.pop();
      auto start = loop_labels.top();
      loop_labels.pop();
      jump_offsets.push_back(a.offset());
      a.b(start);
      a.bind(end);
      break;
    }

    case bfjit::BFOp::Type::SetValue:
      jump_offsets.push_back(a.offset());
      a.mov(CACHE_VALUE, asmjit::Imm(op.set_arg));
      break;
    case bfjit::BFOp::Type::In:
      std::abort();
    case bfjit::BFOp::Type::Halt:
      std::abort();
    }
  }
}
