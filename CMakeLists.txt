cmake_minimum_required(VERSION 3.10)

project(minisandbox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

include_directories("src")

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
        src/sandbox/sandbox.cpp
        src/sandbox/sandbox.h
)