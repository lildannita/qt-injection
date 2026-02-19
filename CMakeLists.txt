cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(qt-injection LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Per-config subdirectories (multi-config generators)
foreach(CFG IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${CFG} CFG_UP)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_BINARY_DIR}/lib")
endforeach()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Quick RemoteObjects Network)

add_subdirectory(examples)
add_subdirectory(agent)

if(WIN32)
    add_subdirectory(platforms/windows_injector)
endif()
