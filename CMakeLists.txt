cmake_minimum_required(VERSION 3.11)

project(minisandbox)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz)
FetchContent_MakeAvailable(json)

set(CMAKE_CXX_STANDARD 20)

include_directories("src")

add_library(
        sandbox
        SHARED
        src/sandbox/sandbox.cpp
        src/sandbox/sandbox.h
)

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE sandbox)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)