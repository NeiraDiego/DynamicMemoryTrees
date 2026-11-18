cmake_minimum_required(VERSION 3.25)

project(dynamic_memory_trees LANGUAGES CXX)

option(DMT_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

add_library(dmt src/dynamic_bitvector.cpp src/tree_visualizer.cpp)
target_include_directories(dmt PUBLIC include)
target_compile_features(dmt PUBLIC cxx_std_23)
target_compile_options(dmt PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(DMT_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(dmt PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(dmt PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_executable(dmt_tests tests/dynamic_bitvector_test.cpp)
target_link_libraries(dmt_tests PRIVATE dmt)
target_compile_options(dmt_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
if(DMT_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(dmt_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(dmt_tests PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME dmt_tests COMMAND dmt_tests)

add_executable(preuba prueba.cpp)
target_link_libraries(preuba PRIVATE dmt)
target_compile_options(preuba PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
if(DMT_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(preuba PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(preuba PRIVATE -fsanitize=address,undefined)
endif()
