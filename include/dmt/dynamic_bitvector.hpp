#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dmt {

struct MemoryStatistics {
  std::size_t useful_bits{};
  std::size_t cell_capacity_bits{};
  std::size_t allocated_cells{};
  std::size_t segments{};
  std::size_t tree_nodes{};
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

  [[nodiscard]] std::vector<bool> to_vector() const;
  [[nodiscard]] MemoryStatistics memory_statistics() const;
  [[nodiscard]] bool check_invariants() const;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace dmt
