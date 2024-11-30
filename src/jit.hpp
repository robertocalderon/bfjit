#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "options.hpp"
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
  using MFuncType = void(__fastcall *)(uint64_t base, uint64_t idx,
                                       uint64_t jump_addr);
#else
  using MFuncType = void (*)(uint64_t base, uint64_t idx, uint64_t jump_addr, void *extra);
#endif
  MFuncType main_function;
  bfjit::CLIOpts const &m_cli_opts;
  struct InnerData;
  std::unique_ptr<InnerData> m_inner_data;

  JIT(std::span<BFOp const> bytecode, bfjit::CLIOpts const &cli_opts);
  ~JIT();
  JIT(JIT const &) = delete;
  JIT(JIT &&) = delete;
  JIT &operator=(JIT const &) = delete;
  JIT &operator=(JIT &&) = delete;

  void run_until_end();
  void do_codegen();
};

} // namespace bfjit
