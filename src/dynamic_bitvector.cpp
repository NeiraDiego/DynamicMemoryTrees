#include "dmt/dynamic_bitvector.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <memory>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dmt {
namespace {

constexpr std::size_t kMaxSegmentBits = 256;
constexpr std::size_t kHandleOrdinalBits = 23;
constexpr std::uint32_t kHandleOrdinalMask = (1U << kHandleOrdinalBits) - 1U;

struct Node;

struct Handle {
  std::uint32_t raw{};

  [[nodiscard]] std::size_t class_index() const noexcept { return raw >> kHandleOrdinalBits; }
  [[nodiscard]] std::size_t ordinal() const noexcept { return raw & kHandleOrdinalMask; }
};

struct Cell {
  std::array<std::uint64_t, 8> words{};
};

struct Pool {
  std::size_t segment_length{};
  std::size_t cell_bits{};
  std::vector<Cell> cells;
  std::vector<Node*> owners;

  [[nodiscard]] bool get(std::size_t ordinal, std::size_t bit) const {
    const std::size_t absolute = ordinal * segment_length + bit;
    const std::size_t cell = absolute / cell_bits;
    const std::size_t in_cell = absolute % cell_bits;
    return (cells[cell].words[in_cell / 64U] >> (in_cell % 64U)) & 1U;
  }

  void set(std::size_t ordinal, std::size_t bit, bool value) {
    const std::size_t absolute = ordinal * segment_length + bit;
    const std::size_t cell = absolute / cell_bits;
    const std::size_t in_cell = absolute % cell_bits;
    const std::uint64_t mask = std::uint64_t{1} << (in_cell % 64U);
    if (value) {
      cells[cell].words[in_cell / 64U] |= mask;
    } else {
      cells[cell].words[in_cell / 64U] &= ~mask;
    }
  }

  void ensure_capacity(std::size_t entries) {
    const std::size_t bits = entries * segment_length;
    const std::size_t needed = (bits + cell_bits - 1U) / cell_bits;
    cells.resize(needed);
  }
};

struct Node {
  Handle handle{};
  std::size_t length{};
  std::size_t total{};
  std::size_t ones{};
  std::int64_t excess{};
  std::int64_t minimum{};
  std::int64_t maximum{};
  std::size_t minimum_count{};
  int height{1};
  std::unique_ptr<Node> left;
  std::unique_ptr<Node> right;

  [[nodiscard]] bool leaf() const noexcept { return !left && !right; }
};

[[nodiscard]] int height(const std::unique_ptr<Node>& node) {
  return node ? node->height : 0;
}

[[nodiscard]] std::size_t total(const std::unique_ptr<Node>& node) {
  return node ? node->total : 0;
}

}  // namespace

struct DynamicBitVector::Impl {
  explicit Impl(std::uint16_t requested_l) : l(requested_l), cell_bits(2U * l) {
    if (l == 0 || l > kMaxSegmentBits) {
      throw std::invalid_argument("segment_bits must be in [1, 256]");
    }
    pools.reserve(cell_bits);
    for (std::size_t length = 1; length <= cell_bits; ++length) {
      pools.push_back(Pool{length, cell_bits, {}, {}});
    }
  }

  std::size_t l;
  std::size_t cell_bits;
  std::vector<Pool> pools;
  std::unique_ptr<Node> root;

  [[nodiscard]] Pool& pool(Handle handle) { return pools.at(handle.class_index()); }
  [[nodiscard]] const Pool& pool(Handle handle) const { return pools.at(handle.class_index()); }

  [[nodiscard]] Handle allocate(Node* owner, const std::vector<bool>& bits) {
    if (bits.empty() || bits.size() > cell_bits) {
      throw std::logic_error("invalid segment length");
    }
    const std::size_t class_index = bits.size() - 1U;
    Pool& target = pools[class_index];
    if (target.owners.size() > kHandleOrdinalMask) {
      throw std::overflow_error("relative segment handle capacity exhausted");
    }
    const std::size_t ordinal = target.owners.size();
    target.ensure_capacity(ordinal + 1U);
    target.owners.push_back(owner);
    for (std::size_t bit = 0; bit < bits.size(); ++bit) {
      target.set(ordinal, bit, bits[bit]);
    }
    return Handle{static_cast<std::uint32_t>((class_index << kHandleOrdinalBits) | ordinal)};
  }

  void release(Handle handle) {
    Pool& source = pool(handle);
    const std::size_t ordinal = handle.ordinal();
    const std::size_t last = source.owners.size() - 1U;
    if (ordinal != last) {
      for (std::size_t bit = 0; bit < source.segment_length; ++bit) {
        source.set(ordinal, bit, source.get(last, bit));
      }
      Node* moved = source.owners[last];
      source.owners[ordinal] = moved;
      moved->handle.raw = static_cast<std::uint32_t>((handle.class_index() << kHandleOrdinalBits) | ordinal);
    }
    source.owners.pop_back();
  }

  [[nodiscard]] std::vector<bool> read(Handle handle, std::size_t length) const {
    const Pool& source = pool(handle);
    std::vector<bool> bits(length);
    for (std::size_t bit = 0; bit < length; ++bit) {
      bits[bit] = source.get(handle.ordinal(), bit);
    }
    return bits;
  }

  [[nodiscard]] RangeMinMaxSummary summarize(Handle handle, std::size_t length) const {
    RangeMinMaxSummary result{0, std::numeric_limits<std::int64_t>::max(),
                              std::numeric_limits<std::int64_t>::min(), 0};
    const Pool& source = pool(handle);
    for (std::size_t bit = 0; bit < length; ++bit) {
      result.excess += source.get(handle.ordinal(), bit) ? 1 : -1;
      if (result.excess < result.minimum) {
        result.minimum = result.excess;
        result.minimum_count = 1;
      } else if (result.excess == result.minimum) {
        ++result.minimum_count;
      }
      result.maximum = std::max(result.maximum, result.excess);
    }
    return result;
  }

  void refresh(Node& node) {
    node.height = 1 + std::max(height(node.left), height(node.right));
    if (node.leaf()) {
      node.total = node.length;
      const RangeMinMaxSummary summary = summarize(node.handle, node.length);
      node.excess = summary.excess;
      node.minimum = summary.minimum;
      node.maximum = summary.maximum;
      node.minimum_count = summary.minimum_count;
      node.ones = static_cast<std::size_t>((node.length + node.excess) / 2);
      return;
    }
    node.total = total(node.left) + total(node.right);
    node.ones = node.left->ones + node.right->ones;
    node.excess = node.left->excess + node.right->excess;
    node.minimum = std::min(node.left->minimum, node.left->excess + node.right->minimum);
    node.maximum = std::max(node.left->maximum, node.left->excess + node.right->maximum);
    if (node.left->minimum < node.left->excess + node.right->minimum) {
      node.minimum_count = node.left->minimum_count;
    } else if (node.left->minimum > node.left->excess + node.right->minimum) {
      node.minimum_count = node.right->minimum_count;
    } else {
      node.minimum_count = node.left->minimum_count + node.right->minimum_count;
    }
  }

  void replace(Node& node, const std::vector<bool>& bits) {
    const Handle old = node.handle;
    node.length = bits.size();
    node.handle = allocate(&node, bits);
    release(old);
    refresh(node);
  }

  [[nodiscard]] std::unique_ptr<Node> make_leaf(const std::vector<bool>& bits) {
    auto node = std::make_unique<Node>();
    node->length = bits.size();
    node->total = bits.size();
    node->handle = allocate(node.get(), bits);
    refresh(*node);
    return node;
  }

  void release_tree(const std::unique_ptr<Node>& node) {
    if (!node) {
      return;
    }
    if (node->leaf()) {
      release(node->handle);
      return;
    }
    release_tree(node->left);
    release_tree(node->right);
  }

  [[nodiscard]] std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> node) {
    auto pivot = std::move(node->right);
    node->right = std::move(pivot->left);
    refresh(*node);
    pivot->left = std::move(node);
    refresh(*pivot);
    return pivot;
  }

  [[nodiscard]] std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> node) {
    auto pivot = std::move(node->left);
    node->left = std::move(pivot->right);
    refresh(*node);
    pivot->right = std::move(node);
    refresh(*pivot);
    return pivot;
  }

  [[nodiscard]] std::unique_ptr<Node> balance(std::unique_ptr<Node> node) {
    refresh(*node);
    const int factor = height(node->left) - height(node->right);
    if (factor > 1) {
      if (height(node->left->left) < height(node->left->right)) {
        node->left = rotate_left(std::move(node->left));
      }
      return rotate_right(std::move(node));
    }
    if (factor < -1) {
      if (height(node->right->right) < height(node->right->left)) {
        node->right = rotate_right(std::move(node->right));
      }
      return rotate_left(std::move(node));
    }
    return node;
  }

  [[nodiscard]] std::unique_ptr<Node> join(std::unique_ptr<Node> left, std::unique_ptr<Node> right) {
    if (!left) {
      return right;
    }
    if (!right) {
      return left;
    }
    if (height(left) > height(right) + 1) {
      left->right = join(std::move(left->right), std::move(right));
      return balance(std::move(left));
    }
    if (height(right) > height(left) + 1) {
      right->left = join(std::move(left), std::move(right->left));
      return balance(std::move(right));
    }
    auto node = std::make_unique<Node>();
    node->left = std::move(left);
    node->right = std::move(right);
    return balance(std::move(node));
  }

  void insert_at(std::unique_ptr<Node>& node, std::size_t position, bool value) {
    if (!node) {
      node = make_leaf(std::vector<bool>{value});
      return;
    }
    if (node->leaf()) {
      auto bits = read(node->handle, node->length);
      bits.insert(bits.begin() + static_cast<std::ptrdiff_t>(position), value);
      if (bits.size() <= cell_bits) {
        replace(*node, bits);
        return;
      }
      std::vector<bool> left_bits(bits.begin(), bits.begin() + static_cast<std::ptrdiff_t>(l));
      std::vector<bool> right_bits(bits.begin() + static_cast<std::ptrdiff_t>(l), bits.end());
      release(node->handle);
      node = join(make_leaf(left_bits), make_leaf(right_bits));
      return;
    }
    const std::size_t left_total = total(node->left);
    if (position <= left_total) {
      insert_at(node->left, position, value);
    } else {
      insert_at(node->right, position - left_total, value);
    }
    node = balance(std::move(node));
  }

  void set_at(std::unique_ptr<Node>& node, std::size_t position, bool value) {
    if (node->leaf()) {
      Pool& source = pool(node->handle);
      source.set(node->handle.ordinal(), position, value);
      refresh(*node);
      return;
    }
    const std::size_t left_total = total(node->left);
    if (position < left_total) {
      set_at(node->left, position, value);
    } else {
      set_at(node->right, position - left_total, value);
    }
    refresh(*node);
  }

  void erase_at(std::unique_ptr<Node>& node, std::size_t position) {
    if (node->leaf()) {
      auto bits = read(node->handle, node->length);
      bits.erase(bits.begin() + static_cast<std::ptrdiff_t>(position));
      if (bits.empty()) {
        release(node->handle);
        node.reset();
      } else {
        replace(*node, bits);
      }
      return;
    }
    const std::size_t left_total = total(node->left);
    if (position < left_total) {
      erase_at(node->left, position);
    } else {
      erase_at(node->right, position - left_total);
    }
    if (!node->left) {
      node = std::move(node->right);
    } else if (!node->right) {
      node = std::move(node->left);
    } else {
      node = balance(std::move(node));
    }
  }

  [[nodiscard]] bool has_underfull_leaf(const std::unique_ptr<Node>& node) const {
    if (!node) {
      return false;
    }
    if (node->leaf()) {
      return node->length < l;
    }
    return has_underfull_leaf(node->left) || has_underfull_leaf(node->right);
  }

  void append_bits(const std::unique_ptr<Node>& node, std::vector<bool>& output) const {
    if (!node) {
      return;
    }
    if (node->leaf()) {
      const auto bits = read(node->handle, node->length);
      output.insert(output.end(), bits.begin(), bits.end());
      return;
    }
    append_bits(node->left, output);
    append_bits(node->right, output);
  }

  void rebuild() {
    std::vector<bool> bits;
    bits.reserve(total(root));
    append_bits(root, bits);
    release_tree(root);
    root.reset();
    if (bits.empty()) {
      return;
    }
    std::vector<std::vector<bool>> parts;
    for (std::size_t offset = 0; offset < bits.size(); offset += l) {
      const std::size_t count = std::min(l, bits.size() - offset);
      parts.emplace_back(bits.begin() + static_cast<std::ptrdiff_t>(offset),
                         bits.begin() + static_cast<std::ptrdiff_t>(offset + count));
    }
    if (parts.size() > 1U && parts.back().size() < l) {
      auto& previous = parts[parts.size() - 2U];
      previous.insert(previous.end(), parts.back().begin(), parts.back().end());
      parts.pop_back();
    }
    for (const auto& part : parts) {
      root = root ? join(std::move(root), make_leaf(part)) : make_leaf(part);
    }
  }

  [[nodiscard]] const Node& locate(const Node& node, std::size_t position, std::size_t& local) const {
    if (node.leaf()) {
      local = position;
      return node;
    }
    const std::size_t left_total = total(node.left);
    if (position < left_total) {
      return locate(*node.left, position, local);
    }
    return locate(*node.right, position - left_total, local);
  }

  [[nodiscard]] std::int64_t prefix_excess(const Node& node, std::size_t count) const {
    if (count == 0) {
      return 0;
    }
    if (count == node.total) {
      return node.excess;
    }
    if (node.leaf()) {
      std::int64_t result{};
      const Pool& source = pool(node.handle);
      for (std::size_t bit = 0; bit < count; ++bit) {
        result += source.get(node.handle.ordinal(), bit) ? 1 : -1;
      }
      return result;
    }
    const std::size_t left_total = total(node.left);
    if (count <= left_total) {
      return prefix_excess(*node.left, count);
    }
    return node.left->excess + prefix_excess(*node.right, count - left_total);
  }

  [[nodiscard]] std::size_t prefix_ones(const Node& node, std::size_t count) const {
    if (count == 0) {
      return 0;
    }
    if (count == node.total) {
      return node.ones;
    }
    if (node.leaf()) {
      std::size_t result{};
      const Pool& source = pool(node.handle);
      for (std::size_t bit = 0; bit < count; ++bit) {
        result += source.get(node.handle.ordinal(), bit) ? 1U : 0U;
      }
      return result;
    }
    const std::size_t left_total = total(node.left);
    if (count <= left_total) {
      return prefix_ones(*node.left, count);
    }
    return node.left->ones + prefix_ones(*node.right, count - left_total);
  }

  [[nodiscard]] std::optional<std::size_t> select_one(const Node& node, std::size_t occurrence,
                                                       std::size_t offset) const {
    if (node.leaf()) {
      const Pool& source = pool(node.handle);
      for (std::size_t bit = 0; bit < node.length; ++bit) {
        if (source.get(node.handle.ordinal(), bit) && occurrence-- == 0) {
          return offset + bit;
        }
      }
      return std::nullopt;
    }
    if (occurrence < node.left->ones) {
      return select_one(*node.left, occurrence, offset);
    }
    return select_one(*node.right, occurrence - node.left->ones, offset + node.left->total);
  }

  [[nodiscard]] std::optional<std::size_t> find_excess(const Node& node, std::size_t offset,
                                                        std::size_t begin, std::int64_t prefix,
                                                        std::int64_t target) const {
    if (offset + node.total <= begin) {
      return std::nullopt;
    }
    if (offset >= begin && (target < prefix + node.minimum || target > prefix + node.maximum)) {
      return std::nullopt;
    }
    if (node.leaf()) {
      const Pool& source = pool(node.handle);
      for (std::size_t bit = 0; bit < node.length; ++bit) {
        prefix += source.get(node.handle.ordinal(), bit) ? 1 : -1;
        if (offset + bit >= begin && prefix == target) {
          return offset + bit;
        }
      }
      return std::nullopt;
    }
    if (const auto result = find_excess(*node.left, offset, begin, prefix, target)) {
      return result;
    }
    return find_excess(*node.right, offset + node.left->total, begin, prefix + node.left->excess, target);
  }

  [[nodiscard]] bool validate(const std::unique_ptr<Node>& node, bool is_root, std::size_t& nodes) const {
    if (!node) {
      return true;
    }
    ++nodes;
    if (node->leaf()) {
      if ((!is_root && (node->length < l || node->length > cell_bits)) || node->total != node->length) {
        return false;
      }
      const Pool& source = pool(node->handle);
      const RangeMinMaxSummary summary = summarize(node->handle, node->length);
      return source.segment_length == node->length && node->handle.ordinal() < source.owners.size() &&
             source.owners[node->handle.ordinal()] == node.get() && node->excess == summary.excess &&
             node->minimum == summary.minimum && node->maximum == summary.maximum &&
             node->minimum_count == summary.minimum_count &&
             node->ones == static_cast<std::size_t>((node->length + node->excess) / 2);
    }
    if (!node->left || !node->right || node->total != total(node->left) + total(node->right) ||
        node->ones != node->left->ones + node->right->ones || node->excess != node->left->excess + node->right->excess ||
        node->minimum != std::min(node->left->minimum, node->left->excess + node->right->minimum) ||
        node->maximum != std::max(node->left->maximum, node->left->excess + node->right->maximum)) {
      return false;
    }
    const std::size_t expected_minimum_count = node->left->minimum < node->left->excess + node->right->minimum
                                                   ? node->left->minimum_count
                                                   : node->left->minimum > node->left->excess + node->right->minimum
                                                         ? node->right->minimum_count
                                                         : node->left->minimum_count + node->right->minimum_count;
    if (node->minimum_count != expected_minimum_count) {
      return false;
    }
    if (std::abs(height(node->left) - height(node->right)) > 1 ||
        node->height != 1 + std::max(height(node->left), height(node->right))) {
      return false;
    }
    return validate(node->left, false, nodes) && validate(node->right, false, nodes);
  }
};

DynamicBitVector::DynamicBitVector(std::uint16_t segment_bits) : impl_(new Impl(segment_bits)) {}

DynamicBitVector::~DynamicBitVector() {
  if (impl_) {
    impl_->release_tree(impl_->root);
    delete impl_;
  }
}

DynamicBitVector::DynamicBitVector(DynamicBitVector&& other) noexcept : impl_(std::exchange(other.impl_, nullptr)) {}

DynamicBitVector& DynamicBitVector::operator=(DynamicBitVector&& other) noexcept {
  if (this != &other) {
    if (impl_) {
      impl_->release_tree(impl_->root);
      delete impl_;
    }
    impl_ = std::exchange(other.impl_, nullptr);
  }
  return *this;
}

std::size_t DynamicBitVector::size() const noexcept { return total(impl_->root); }
bool DynamicBitVector::empty() const noexcept { return size() == 0; }

bool DynamicBitVector::at(std::size_t position) const {
  if (position >= size()) {
    throw std::out_of_range("bit position out of range");
  }
  std::size_t local{};
  const Node& leaf = impl_->locate(*impl_->root, position, local);
  return impl_->pool(leaf.handle).get(leaf.handle.ordinal(), local);
}

void DynamicBitVector::set(std::size_t position, bool value) {
  if (position >= size()) {
    throw std::out_of_range("bit position out of range");
  }
  impl_->set_at(impl_->root, position, value);
}

void DynamicBitVector::insert(std::size_t position, bool value) {
  if (position > size()) {
    throw std::out_of_range("insert position out of range");
  }
  impl_->insert_at(impl_->root, position, value);
}

void DynamicBitVector::erase(std::size_t position) {
  if (position >= size()) {
    throw std::out_of_range("bit position out of range");
  }
  impl_->erase_at(impl_->root, position);
  if (impl_->root && impl_->has_underfull_leaf(impl_->root)) {
    impl_->rebuild();
  }
}

RangeMinMaxSummary DynamicBitVector::range_min_max() const noexcept {
  if (!impl_->root) {
    return {};
  }
  return {impl_->root->excess, impl_->root->minimum, impl_->root->maximum, impl_->root->minimum_count};
}

std::int64_t DynamicBitVector::excess_before(std::size_t position) const {
  if (position > size()) {
    throw std::out_of_range("prefix position out of range");
  }
  return impl_->root ? impl_->prefix_excess(*impl_->root, position) : 0;
}

std::size_t DynamicBitVector::rank1(std::size_t position) const {
  if (position > size()) {
    throw std::out_of_range("prefix position out of range");
  }
  return impl_->root ? impl_->prefix_ones(*impl_->root, position) : 0;
}

std::optional<std::size_t> DynamicBitVector::select1(std::size_t occurrence) const {
  if (!impl_->root || occurrence >= impl_->root->ones) {
    return std::nullopt;
  }
  return impl_->select_one(*impl_->root, occurrence, 0);
}

std::optional<std::size_t> DynamicBitVector::find_first_excess(std::size_t begin, std::int64_t target) const {
  if (begin > size()) {
    throw std::out_of_range("search position out of range");
  }
  if (!impl_->root || begin == size()) {
    return std::nullopt;
  }
  return impl_->find_excess(*impl_->root, 0, begin, 0, target);
}

std::optional<std::size_t> DynamicBitVector::find_close(std::size_t opening_position) const {
  if (opening_position >= size()) {
    throw std::out_of_range("opening position out of range");
  }
  if (!at(opening_position)) {
    return std::nullopt;
  }
  return find_first_excess(opening_position + 1U, excess_before(opening_position));
}

std::vector<bool> DynamicBitVector::to_vector() const {
  std::vector<bool> result;
  result.reserve(size());
  impl_->append_bits(impl_->root, result);
  return result;
}

MemoryStatistics DynamicBitVector::memory_statistics() const {
  MemoryStatistics result;
  result.useful_bits = size();
  for (const Pool& pool : impl_->pools) {
    result.allocated_cells += pool.cells.size();
    result.cell_capacity_bits += pool.cells.size() * impl_->cell_bits;
    result.segments += pool.owners.size();
  }
  std::size_t nodes{};
  (void)impl_->validate(impl_->root, true, nodes);
  result.tree_nodes = nodes;
  return result;
}

bool DynamicBitVector::check_invariants() const {
  std::size_t nodes{};
  return impl_->validate(impl_->root, true, nodes);
}

}  // namespace dmt
