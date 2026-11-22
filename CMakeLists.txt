cmake_minimum_required(VERSION 3.20)
project(osu_pp LANGUAGES CXX)

# The library: one header, include/osu_pp/difficulty.h. osu! never fuses
# floating-point operations it doesn't ask to, so neither may the compiler:
# targets using it get -ffp-contract=off.
add_library(osu_pp INTERFACE)
add_library(osu_pp::osu_pp ALIAS osu_pp)
target_include_directories(osu_pp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(osu_pp INTERFACE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(osu_pp INTERFACE -ffp-contract=off)
endif()

# The comparison with osu!'s own calculation (tests/run.sh), when this is
# the project being built rather than one using it.
if(PROJECT_IS_TOP_LEVEL)
  add_executable(compare tests/compare.cpp)
  target_link_libraries(compare PRIVATE osu_pp)
  target_compile_options(compare PRIVATE -Wall -Wextra -Werror)
endif()
