#pragma once
#include "minic/ast.hpp"

namespace minic {
struct OptimizationStats {
    std::size_t folded = 0;
    std::size_t removed = 0;
};
OptimizationStats optimize(Program& program);
} // namespace minic
