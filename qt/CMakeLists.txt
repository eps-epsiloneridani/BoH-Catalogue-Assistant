# BoH Librarian — Qt port (Linux). Plan: docs/superpowers/plans/2026-10-09-linux-qt-port.md
# Stack: C++20, Qt 6.2+ Widgets, system libsqlite3. No other dependencies (D2 spirit).
cmake_minimum_required(VERSION 3.21)
project(BoHLibrarian VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Test)
find_package(SQLite3 REQUIRED)

# bohcore (models, SQLite layer, migrator, repositories) arrives in Task 2 with its
# first sources — CMake cannot generate a zero-source static library.

add_executable(boh-librarian
  src/app/main.cpp
)
target_link_libraries(boh-librarian PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

enable_testing()

add_executable(test_smoke tests/test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE Qt6::Core Qt6::Test)
add_test(NAME smoke COMMAND test_smoke)
