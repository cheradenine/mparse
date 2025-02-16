cmake_minimum_required(VERSION 3.22)  # Specify minimum CMake version
cmake_policy(SET CMP0135 NEW)

project(mparse)  # Set the project name

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        6910c9d9165801d8827d628cb72eb7ea9dd538c5
)
FetchContent_MakeAvailable(googletest)

# Add an executable target
add_executable(mparse main.cpp)

enable_testing()

add_executable(
  mparse_test
  mparse_test.cpp
)

target_link_libraries(
  mparse_test
  GTest::gtest_main
  GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(mparse_test)
