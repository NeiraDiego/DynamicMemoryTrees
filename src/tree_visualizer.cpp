#include "dmt/tree_visualizer.hpp"

namespace dmt {

std::string visualize_small_tree(const DynamicBitVector& bits, std::size_t max_bits) {
  return bits.debug_tree(max_bits);
}

}  // namespace dmt
