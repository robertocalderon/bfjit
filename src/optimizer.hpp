#pragma once

#include "parser.hpp"
#include <span>
#include <vector>

namespace bfjit {

    auto one_step_optimize(std::span<BFOp const> buffer, bool *did_something = nullptr) -> std::vector<BFOp>;
    auto optimize(std::span<BFOp> buffer) -> std::vector<BFOp>;
    void do_loop_relink(std::span<BFOp> buffer);

}