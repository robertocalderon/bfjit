#pragma once

#include <vector>
#include <string_view>

namespace bfjit {

    struct BFOp {
        enum class Type {
            Mod,
            ModPtr,
            In,
            Out,
            LoopBeg,
            LoopEnd,

            // Optimized operations
            SetValue,
            Halt,
        } m_type;
        enum class HaltReason {
            InfiniteLoop
        };
        union {
            uint8_t inc_arg;
            uint8_t set_arg;
            int64_t inc_ptr_arg;
            size_t loop_arg;
            HaltReason halt_reason;
        };
    };

    [[nodiscard]]
    auto skip_comment(std::string_view program) -> std::string_view;
    [[nodiscard]]
    auto parse_program(std::string_view program) -> std::vector<BFOp>;

}
