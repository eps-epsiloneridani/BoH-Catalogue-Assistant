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
  src/core/Migrator.cpp
  src/core/Models.cpp
  src/core/ReadingMath.cpp
  src/core/ColorMath.cpp
  src/core/SaveImport.cpp
  src/core/BookQuery.cpp
  src/core/MemoryQuery.cpp
  src/core/SkillQuery.cpp
  src/core/Repositories/PlaythroughRepository.cpp
  src/core/Repositories/LookupRepositories.cpp
  src/core/Repositories/BookRepository.cpp
  src/core/Repositories/MemoryRepository.cpp
  src/core/Repositories/SkillRepository.cpp
  src/core/Repositories/JournalRepository.cpp
)
target_include_directories(bohcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core
                                          ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Repositories)
# db/migrations embedded verbatim — single schema source, no sync copy (plan
# Task 3). qt_add_resources (not a checked-in .qrc): rcc's relative-path rules
# don't reach outside the target tree, CMake resolves these paths itself.
# Files outside the target dir need an explicit QT_RESOURCE_ALIAS (their name
# under :/migrations/).
set(MIGRATION_FILES
  ../db/migrations/001_initial_schema.sql
  ../db/migrations/002_seed_principles.sql
  ../db/migrations/003_seed_languages.sql
  ../db/migrations/004_rename_difficulty.sql
  ../db/migrations/005_playthroughs.sql
  ../db/migrations/006_per_playthrough_unique_names.sql
  ../db/migrations/007_fix_imported_book_kinds.sql
  ../db/migrations/008_humanize_transit_locations.sql
  ../db/migrations/009_remove_unacquired_auction_books.sql
)
foreach(migration_file IN ITEMS ${MIGRATION_FILES})
  get_filename_component(alias "${migration_file}" NAME)
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${migration_file}"
    PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()
qt_add_resources(bohcore "migrations"
  PREFIX "/migrations"
  FILES ${MIGRATION_FILES}
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

add_executable(test_Migrator tests/test_Migrator.cpp)
target_link_libraries(test_Migrator PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME Migrator COMMAND test_Migrator)

add_executable(test_ColorMath tests/test_ColorMath.cpp)
target_link_libraries(test_ColorMath PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME ColorMath COMMAND test_ColorMath)

add_executable(test_ReadingMath tests/test_ReadingMath.cpp)
target_link_libraries(test_ReadingMath PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME ReadingMath COMMAND test_ReadingMath)

add_executable(test_BookQuery tests/test_BookQuery.cpp)
target_link_libraries(test_BookQuery PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME BookQuery COMMAND test_BookQuery)

add_executable(test_MemoryQuery tests/test_MemoryQuery.cpp)
target_link_libraries(test_MemoryQuery PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME MemoryQuery COMMAND test_MemoryQuery)

add_executable(test_SkillQuery tests/test_SkillQuery.cpp)
target_link_libraries(test_SkillQuery PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME SkillQuery COMMAND test_SkillQuery)

add_executable(test_Repositories tests/test_Repositories.cpp)
target_link_libraries(test_Repositories PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME Repositories COMMAND test_Repositories)

add_executable(test_PlaythroughRepository tests/test_PlaythroughRepository.cpp)
target_link_libraries(test_PlaythroughRepository PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME PlaythroughRepository COMMAND test_PlaythroughRepository)

add_executable(test_SaveImport tests/test_SaveImport.cpp)
target_link_libraries(test_SaveImport PRIVATE bohcore Qt6::Core Qt6::Test)
add_test(NAME SaveImport COMMAND test_SaveImport)
