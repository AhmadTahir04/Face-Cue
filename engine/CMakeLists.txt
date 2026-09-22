cmake_minimum_required(VERSION 3.16)
project(ffa_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Match Homebrew's dylib deployment target to silence linker version warnings.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(COMMAND sw_vers -productVersion
                    OUTPUT_VARIABLE MACOS_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCH "^[0-9]+" MACOS_MAJOR "${MACOS_VER}")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${MACOS_MAJOR}.0")
endif()

# Homebrew (Apple Silicon) install prefix, so find_package locates OpenCV etc.
if(APPLE AND EXISTS "/opt/homebrew")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(OpenCV REQUIRED)
find_package(SQLite3 REQUIRED)

# --- shared engine library ---
add_library(ffacore STATIC
    src/camera.cpp
    src/recognizer.cpp
    src/storage.cpp
    src/safety.cpp
    src/announcer.cpp
    src/pipeline.cpp
)
target_include_directories(ffacore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(ffacore PUBLIC ${OpenCV_LIBS} SQLite3::SQLite3)

# --- main app (CLI prototype) ---
add_executable(ffa src/main.cpp)
target_link_libraries(ffa PRIVATE ffacore)

# --- evaluation / threshold-tuning tool ---
add_executable(ffa_eval src/eval.cpp)
target_link_libraries(ffa_eval PRIVATE ffacore)

# --- unit tests (safety logic; no ML model needed) ---
enable_testing()
find_package(GTest)
if(GTest_FOUND)
    add_executable(ffa_tests tests/test_safety.cpp)
    target_link_libraries(ffa_tests PRIVATE ffacore GTest::gtest GTest::gtest_main)
    add_test(NAME ffa_tests COMMAND ffa_tests)
else()
    message(STATUS "GTest not found — skipping unit tests. `brew install googletest`")
endif()
