
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

struct EHandler : public asmjit::ErrorHandler {
  void handleError(asmjit::Error err, char const *msg,
                   asmjit::BaseEmitter *) override {
    fmt::print("asmjit error: {} ({})", msg, err);
    std::abort();
  }
};

void print_char(uint64_t r) {
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

  ::do_codegen(a, m_bytecode, exit_label, outside_of_bounds,
               (uint32_t)this->m_buffer.size(), this->mapping_bytecode_to_code);

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
      std::abort();
    case bfjit::BFOp::Type::ModPtr:
      std::abort();
    case bfjit::BFOp::Type::Out:
      std::abort();
    case bfjit::BFOp::Type::LoopBeg:
      std::abort();
    case bfjit::BFOp::Type::LoopEnd:
      std::abort();
    case bfjit::BFOp::Type::SetValue:
      std::abort();
    case bfjit::BFOp::Type::In:
      std::abort();
    case bfjit::BFOp::Type::Halt:
      std::abort();
    }
  }
}
