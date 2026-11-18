#include "dmt/dynamic_bitvector.hpp"
#include "dmt/tree_visualizer.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
  dmt::DynamicBitVector bits(64);
  const auto print_data = [&bits]() {
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
  };

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

  std::cout << "Initial data:\n";
  print_data();
  std::cout << "\nTree:\n" << dmt::visualize_small_tree(bits);

  bits.set(5, true);
  bits.set(6, false);
  std::cout << "\nTree after changing bits 5 and 6:\n" << dmt::visualize_small_tree(bits);

  // Erase the higher index first so these remove the original 1,0 pair.
  bits.erase(3);
  bits.erase(2);
  std::cout << "\nTree after erasing bits 2 and 3:\n" << dmt::visualize_small_tree(bits);

  // Index 12 is 0 after the deletions; insert a new balanced pair after it.
  bits.insert(11, true);
  bits.insert(12, false);
  std::cout << "\nFinal data:\n" << dmt::visualize_small_tree(bits);
  print_data();
}
