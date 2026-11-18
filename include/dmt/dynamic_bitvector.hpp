#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dmt {

class DynamicBitVector;

// Produces an ASCII representation of the internal AVL tree for debugging.
// The function reports a concise refusal when the vector exceeds max_bits.
[[nodiscard]] std::string visualize_small_tree(const DynamicBitVector& bits, std::size_t max_bits = 256);

struct MemoryStatistics {
  std::size_t useful_bits{};
  std::size_t cell_capacity_bits{};
  std::size_t allocated_cells{};
  std::size_t segments{};
  std::size_t tree_nodes{};
};

// Values are relative to the beginning of a subtree, with bit 1 = +1 and
// bit 0 = -1. minimum_count counts occurrences after consuming a bit.
struct RangeMinMaxSummary {
  std::int64_t excess{};
  std::int64_t minimum{};
  std::int64_t maximum{};
  std::size_t minimum_count{};
};

class DynamicBitVector {
 public:
  // segment_bits is L. It must be in [1, 256] so a 32-bit relative handle
  // can encode both the size class and its ordinal.
  explicit DynamicBitVector(std::uint16_t segment_bits = 64);
  ~DynamicBitVector();

  DynamicBitVector(const DynamicBitVector&) = delete;
  DynamicBitVector& operator=(const DynamicBitVector&) = delete;
  DynamicBitVector(DynamicBitVector&&) noexcept;
  DynamicBitVector& operator=(DynamicBitVector&&) noexcept;

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] bool at(std::size_t position) const;
  void set(std::size_t position, bool value);
  void insert(std::size_t position, bool value);
  void erase(std::size_t position);

  [[nodiscard]] RangeMinMaxSummary range_min_max() const noexcept;
  [[nodiscard]] std::int64_t excess_before(std::size_t position) const;
  [[nodiscard]] std::size_t rank1(std::size_t position) const;
  [[nodiscard]] std::optional<std::size_t> select1(std::size_t occurrence) const;
  [[nodiscard]] std::optional<std::size_t> find_first_excess(std::size_t begin,
                                                              std::int64_t target) const;
  [[nodiscard]] std::optional<std::size_t> find_close(std::size_t opening_position) const;

  [[nodiscard]] std::vector<bool> to_vector() const;
  [[nodiscard]] MemoryStatistics memory_statistics() const;
  [[nodiscard]] bool check_invariants() const;

 private:
  friend std::string visualize_small_tree(const DynamicBitVector& bits, std::size_t max_bits);
  [[nodiscard]] std::string debug_tree(std::size_t max_bits) const;

  struct Impl;
  Impl* impl_;
};

}  // namespace dmt
