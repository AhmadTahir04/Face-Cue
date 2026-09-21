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

add_executable(ffa
    src/main.cpp
    src/camera.cpp
    src/recognizer.cpp
    src/storage.cpp
    src/safety.cpp
    src/announcer.cpp
)

target_include_directories(ffa PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ffa PRIVATE
    ${OpenCV_LIBS}
    SQLite3::SQLite3
)
