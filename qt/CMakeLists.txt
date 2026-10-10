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

# Core library: models, SQLite layer, migrator, repositories (plan Task 2+).
add_library(bohcore STATIC
  src/core/SQLiteValue.cpp
  src/core/SQLiteDatabase.cpp
)
target_include_directories(bohcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(bohcore PUBLIC Qt6::Core SQLite::SQLite3)

add_executable(boh-librarian
  src/app/main.cpp
)
target_link_libraries(boh-librarian PRIVATE bohcore Qt6::Gui Qt6::Widgets)

enable_testing()

add_executable(test_smoke tests/test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE Qt6::Core Qt6::Test)
add_test(NAME smoke COMMAND test_smoke)

add_executable(test_SQLiteDatabase tests/test_SQLiteDatabase.cpp)
target_link_libraries(test_SQLiteDatabase PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME SQLiteDatabase COMMAND test_SQLiteDatabase)
