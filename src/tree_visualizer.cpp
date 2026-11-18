#include "dmt/tree_visualizer.hpp"

#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dmt {
namespace {

struct TreeNode {
  explicit TreeNode(std::size_t node_id) : id(node_id) {}

  std::size_t id;
  std::size_t depth{};
  double column{};
  std::vector<std::unique_ptr<TreeNode>> children;
};

void assign_layout(TreeNode& node, std::size_t depth, std::size_t& next_leaf) {
  node.depth = depth;
  if (node.children.empty()) {
    node.column = static_cast<double>(next_leaf++);
    return;
  }
  for (auto& child : node.children) {
    assign_layout(*child, depth + 1U, next_leaf);
  }
  node.column = (node.children.front()->column + node.children.back()->column) / 2.0;
}

void collect_by_depth(const TreeNode& node, std::vector<std::vector<const TreeNode*>>& levels) {
  if (levels.size() <= node.depth) {
    levels.resize(node.depth + 1U);
  }
  levels[node.depth].push_back(&node);
  for (const auto& child : node.children) {
    collect_by_depth(*child, levels);
  }
}

[[nodiscard]] std::string trim_right(std::string line) {
  const std::size_t last = line.find_last_not_of(' ');
  return last == std::string::npos ? std::string{} : line.substr(0, last + 1U);
}

}  // namespace

std::string visualize_small_tree(const DynamicBitVector& bits) {
  if (bits.size() < 2U || !bits.at(0) || bits.at(bits.size() - 1U) || bits.range_min_max().excess != 0 ||
      bits.range_min_max().minimum < 0) {
    throw std::invalid_argument("the bitvector is not a valid balanced-parentheses tree");
  }

  auto root = std::make_unique<TreeNode>(1);
  std::vector<TreeNode*> open_nodes{root.get()};
  std::size_t next_id = 2;
  for (std::size_t position = 1; position + 1U < bits.size(); ++position) {
    if (bits.at(position)) {
      auto child = std::make_unique<TreeNode>(next_id++);
      TreeNode* child_pointer = child.get();
      open_nodes.back()->children.push_back(std::move(child));
      open_nodes.push_back(child_pointer);
    } else {
      if (open_nodes.size() == 1U) {
        throw std::invalid_argument("the bitvector closes the root before its final bit");
      }
      open_nodes.pop_back();
    }
  }
  if (open_nodes.size() != 1U) {
    throw std::invalid_argument("the bitvector has unclosed parentheses");
  }

  std::size_t leaf_count{};
  assign_layout(*root, 0, leaf_count);
  std::vector<std::vector<const TreeNode*>> levels;
  collect_by_depth(*root, levels);

  constexpr std::size_t column_width = 8;
  std::ostringstream output;
  for (const auto& level : levels) {
    std::string line(leaf_count * column_width + column_width, ' ');
    for (const TreeNode* node : level) {
      const std::string label = "(" + std::to_string(node->id) + ")";
      const std::size_t center = static_cast<std::size_t>(node->column * static_cast<double>(column_width));
      const std::size_t start = center >= label.size() / 2U ? center - label.size() / 2U : 0;
      line.replace(start, label.size(), label);
    }
    output << trim_right(std::move(line)) << '\n';
  }
  return output.str();
}

}  // namespace dmt
