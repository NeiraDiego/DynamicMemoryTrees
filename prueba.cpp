#include "dmt/dynamic_bitvector.hpp"
#include "dmt/tree_visualizer.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
  dmt::DynamicBitVector bits(64);

  /* Construcción manual de ejemplo, sustituida por la carga desde CSV.
  // 1 representa +1 y 0 representa -1 en las consultas Range Min-Max.
  bits.insert(0, true);   // (
  bits.insert(1, true);   // (
  bits.insert(2, false);  // )
  bits.insert(3, false);  // )

  bits.set(1, false);
  bits.erase(1);
  */

  std::ifstream input("data/miniTree8nodes.csv");
  if (!input) {
    throw std::runtime_error("could not open data/miniTree8nodes.csv");
  }
  for (char character; input.get(character);) {
    if (character == '0' || character == '1') {
      bits.insert(bits.size(), character == '1');
    } else if (character != ',' && character != '\n' && character != '\r' && character != ' ') {
      throw std::runtime_error("el CSV contiene un caracter que no es un bit");
    }
  }
  if (bits.size() != 16U) {
    throw std::runtime_error("the CSV must contain exactly 16 bits");
  }

  std::cout << "size: " << bits.size() << '\n';
  std::cout << "bit 0: " << bits.at(0) << '\n';

  const dmt::RangeMinMaxSummary summary = bits.range_min_max();
  std::cout << "excess: " << summary.excess << '\n';
  std::cout << "minimum: " << summary.minimum << '\n';
  std::cout << "maximum: " << summary.maximum << '\n';
  std::cout << "ones before 3: " << bits.rank1(3) << '\n';

  if (const auto position = bits.select1(0)) {
    std::cout << "first one at: " << *position << '\n';
  }
  if (const auto closing = bits.find_close(0)) {
    std::cout << "matching close: " << *closing << '\n';
  }
  std::cout << "\nTree:\n" << dmt::visualize_small_tree(bits);
}
