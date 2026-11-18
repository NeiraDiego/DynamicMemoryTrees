#include "dmt/dynamic_bitvector.hpp"

#include <cassert>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

void expect_equal(const dmt::DynamicBitVector& actual, const std::vector<bool>& expected) {
  assert(actual.size() == expected.size());
  assert(actual.to_vector() == expected);
  assert(actual.check_invariants());
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

  while (!reference.empty()) {
    const std::size_t position = reference.size() / 3U;
    bits.erase(position);
    reference.erase(reference.begin() + static_cast<std::ptrdiff_t>(position));
    assert(bits.check_invariants());
    expect_equal(bits, reference);
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
  }
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
  bounds_test();
}
