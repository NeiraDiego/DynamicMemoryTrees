#include "dmt/dynamic_bitvector.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

void expect_equal(const dmt::DynamicBitVector& actual, const std::vector<bool>& expected) {
  assert(actual.size() == expected.size());
  assert(actual.to_vector() == expected);
  assert(actual.check_invariants());
}

[[nodiscard]] dmt::RangeMinMaxSummary reference_summary(const std::vector<bool>& bits) {
  if (bits.empty()) {
    return {};
  }
  dmt::RangeMinMaxSummary result{0, std::numeric_limits<std::int64_t>::max(),
                                 std::numeric_limits<std::int64_t>::min(), 0};
  for (const bool bit : bits) {
    result.excess += bit ? 1 : -1;
    if (result.excess < result.minimum) {
      result.minimum = result.excess;
      result.minimum_count = 1;
    } else if (result.minimum == result.excess) {
      ++result.minimum_count;
    }
    result.maximum = std::max(result.maximum, result.excess);
  }
  return result;
}

void expect_queries(const dmt::DynamicBitVector& actual, const std::vector<bool>& expected) {
  const dmt::RangeMinMaxSummary summary = reference_summary(expected);
  assert(actual.range_min_max().excess == summary.excess);
  assert(actual.range_min_max().minimum == summary.minimum);
  assert(actual.range_min_max().maximum == summary.maximum);
  assert(actual.range_min_max().minimum_count == summary.minimum_count);

  std::int64_t excess{};
  std::size_t ones{};
  for (std::size_t position = 0; position <= expected.size(); ++position) {
    assert(actual.excess_before(position) == excess);
    assert(actual.rank1(position) == ones);
    if (position < expected.size()) {
      excess += expected[position] ? 1 : -1;
      ones += expected[position] ? 1U : 0U;
    }
  }
  std::size_t occurrence{};
  for (std::size_t position = 0; position < expected.size(); ++position) {
    if (expected[position]) {
      assert(actual.select1(occurrence++) == position);
    }
  }
  assert(!actual.select1(occurrence));
}

void deterministic_operations() {
  dmt::DynamicBitVector bits(8);
  std::vector<bool> reference;
  for (std::size_t index = 0; index < 40; ++index) {
    const bool value = (index % 3U) == 0U;
    bits.insert(bits.size(), value);
    reference.push_back(value);
    assert(bits.check_invariants());
  }
  expect_equal(bits, reference);
  expect_queries(bits, reference);

  bits.insert(0, true);
  reference.insert(reference.begin(), true);
  assert(bits.check_invariants());
  bits.insert(17, false);
  reference.insert(reference.begin() + 17, false);
  assert(bits.check_invariants());
  bits.set(12, true);
  reference[12] = true;
  assert(bits.check_invariants());
  expect_equal(bits, reference);
  expect_queries(bits, reference);

  while (!reference.empty()) {
    const std::size_t position = reference.size() / 3U;
    bits.erase(position);
    reference.erase(reference.begin() + static_cast<std::ptrdiff_t>(position));
    assert(bits.check_invariants());
    expect_equal(bits, reference);
    expect_queries(bits, reference);
  }
  assert(bits.empty());
}

void randomized_differential_test() {
  dmt::DynamicBitVector bits(16);
  std::vector<bool> reference;
  std::mt19937_64 random(0xD1A6B17ULL);
  for (std::size_t step = 0; step < 20'000; ++step) {
    const std::uint64_t action = random() % 3U;
    if (reference.empty() || action == 0U) {
      const std::size_t position = static_cast<std::size_t>(random() % (reference.size() + 1U));
      const bool value = (random() & 1U) != 0U;
      bits.insert(position, value);
      reference.insert(reference.begin() + static_cast<std::ptrdiff_t>(position), value);
    } else if (action == 1U) {
      const std::size_t position = static_cast<std::size_t>(random() % reference.size());
      bits.erase(position);
      reference.erase(reference.begin() + static_cast<std::ptrdiff_t>(position));
    } else {
      const std::size_t position = static_cast<std::size_t>(random() % reference.size());
      const bool value = (random() & 1U) != 0U;
      bits.set(position, value);
      reference[position] = value;
    }
    expect_equal(bits, reference);
    expect_queries(bits, reference);
  }
}

void balanced_parentheses_queries() {
  dmt::DynamicBitVector bits(2);
  const std::vector<bool> parentheses{true, true, false, true, false, false};
  for (const bool bit : parentheses) {
    bits.insert(bits.size(), bit);
  }
  expect_queries(bits, parentheses);
  assert(bits.find_first_excess(0, 2) == 1U);
  assert(bits.find_first_excess(2, 2) == 3U);
  assert(!bits.find_first_excess(4, 2));
  assert(bits.find_close(0) == 5U);
  assert(bits.find_close(1) == 2U);
  assert(bits.find_close(3) == 4U);
  assert(!bits.find_close(2));
}

void bounds_test() {
  dmt::DynamicBitVector bits;
  bool threw = false;
  try {
    (void)bits.at(0);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);
}

}  // namespace

int main() {
  deterministic_operations();
  randomized_differential_test();
  balanced_parentheses_queries();
  bounds_test();
}
