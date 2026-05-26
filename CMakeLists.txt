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

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(QuantLib CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_executable(01_quantlib_hello_swap examples/01_quantlib_hello_swap.cpp)
target_link_libraries(01_quantlib_hello_swap PRIVATE QuantLib::QuantLib)

enable_testing()

add_executable(irc_smoke_tests tests/test_smoke.cpp)
target_link_libraries(irc_smoke_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(irc_smoke_tests)
