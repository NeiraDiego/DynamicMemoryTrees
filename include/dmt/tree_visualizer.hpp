#pragma once

#include "dmt/dynamic_bitvector.hpp"

#include <string>

namespace dmt {

// Renders the ordinal tree encoded by a balanced-parentheses bitvector.
// Bit 1 opens a node and bit 0 closes it. The outermost pair is the root.
// Throws std::invalid_argument if bits is not one balanced-parentheses tree.
[[nodiscard]] std::string visualize_small_tree(const DynamicBitVector& bits);

}  // namespace dmt
