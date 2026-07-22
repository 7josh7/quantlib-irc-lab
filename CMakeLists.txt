cmake_minimum_required(VERSION 3.20)

project(
  irc
  VERSION 0.1.0
  DESCRIPTION "Interest Rate and Credit Models implementation lab"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IRC_ENABLE_MSVC_COVERAGE "Enable MSVC static native code coverage" OFF)
set(IRC_COVERAGE_MIN_LINE_PERCENT "70" CACHE STRING
    "Minimum src/ line coverage required by the coverage target")

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(IRC_ENABLE_MSVC_COVERAGE)
  if(NOT MSVC)
    message(FATAL_ERROR "IRC_ENABLE_MSVC_COVERAGE requires the MSVC toolchain")
  endif()

  # Microsoft's recommended static native instrumentation requires /PROFILE.
  add_link_options(/PROFILE /INCREMENTAL:NO)
endif()

find_package(QuantLib CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_executable(01_quantlib_hello_swap examples/01_quantlib_hello_swap.cpp)
target_link_libraries(01_quantlib_hello_swap PRIVATE QuantLib::QuantLib)

# Phase 1 — hand-rolled mini pricer (docs/impl_notes/01_mini_pricer.md)
add_library(irc_pricing STATIC
  src/core/flat_curve.cpp
  src/core/linear_interpolator.cpp
  src/curves/curve_instruments.cpp
  src/curves/curve_io.cpp
  src/curves/market_data_io.cpp
  src/curves/piecewise_log_linear_curve.cpp
  src/curves/sofr_bootstrapper.cpp
  src/rates/coupon_period.cpp
  src/rates/rate_accrual.cpp
  src/rates/fixed_leg.cpp
  src/rates/floating_leg.cpp
  src/rates/vanilla_swap.cpp
  src/risk/dv01.cpp
)
target_include_directories(irc_pricing PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(irc_pricing PUBLIC QuantLib::QuantLib)

add_executable(02_sofr_curve_bootstrap examples/02_sofr_curve_bootstrap.cpp)
target_link_libraries(02_sofr_curve_bootstrap PRIVATE irc_pricing)

add_executable(02_quantlib_sofr_curve_bootstrap
  examples/02_quantlib_sofr_curve_bootstrap.cpp)
target_link_libraries(02_quantlib_sofr_curve_bootstrap PRIVATE
  irc_pricing QuantLib::QuantLib)

enable_testing()

add_executable(irc_smoke_tests tests/test_smoke.cpp)
target_link_libraries(irc_smoke_tests PRIVATE GTest::gtest_main)

add_executable(test_mini_pricer tests/test_mini_pricer.cpp)
target_link_libraries(test_mini_pricer PRIVATE irc_pricing GTest::gtest_main)

add_executable(test_curve_bootstrap tests/test_curve_bootstrap.cpp)
target_link_libraries(test_curve_bootstrap PRIVATE irc_pricing GTest::gtest_main)

set(PHASE2_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_data")
file(MAKE_DIRECTORY "${PHASE2_TEST_DATA_DIR}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/data/market/sofr_quotes_2026-01-15.csv"
  "${PHASE2_TEST_DATA_DIR}/sofr_quotes_2026-01-15.csv"
  COPYONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/data/market/sofr_fixings_2025-12-17_2026-01-14.csv"
  "${PHASE2_TEST_DATA_DIR}/sofr_fixings_2025-12-17_2026-01-14.csv"
  COPYONLY)
file(TO_CMAKE_PATH "${PHASE2_TEST_DATA_DIR}" PHASE2_TEST_DATA_DIR_CMAKE)
target_compile_definitions(test_curve_bootstrap PRIVATE
  IRC_TEST_DATA_DIR="${PHASE2_TEST_DATA_DIR_CMAKE}")

set(IRC_TEST_TARGETS irc_smoke_tests test_mini_pricer test_curve_bootstrap)

include(GoogleTest)
gtest_discover_tests(irc_smoke_tests)
gtest_discover_tests(test_mini_pricer)
gtest_discover_tests(test_curve_bootstrap)

if(IRC_ENABLE_MSVC_COVERAGE)
  set(IRC_COVERAGE_TEST_EXECUTABLES)
  foreach(test_target IN LISTS IRC_TEST_TARGETS)
    list(APPEND IRC_COVERAGE_TEST_EXECUTABLES "$<TARGET_FILE:${test_target}>")
  endforeach()
  list(JOIN IRC_COVERAGE_TEST_EXECUTABLES "|" IRC_COVERAGE_TEST_EXECUTABLE_LIST)

  add_custom_target(
    coverage
    COMMAND
      powershell.exe -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_SOURCE_DIR}/cmake/run_msvc_coverage.ps1"
      -SourceDirectory "${CMAKE_SOURCE_DIR}"
      -BuildDirectory "${CMAKE_BINARY_DIR}"
      -MinimumLinePercent "${IRC_COVERAGE_MIN_LINE_PERCENT}"
      -TestExecutableList "${IRC_COVERAGE_TEST_EXECUTABLE_LIST}"
    DEPENDS ${IRC_TEST_TARGETS}
    USES_TERMINAL
    VERBATIM
  )
endif()
