cmake_minimum_required(VERSION 3.11)

project(minisandbox)

set(CMAKE_CXX_STANDARD 20)

include_directories("src")

add_library(
        sandbox
        SHARED
        src/sandbox/sandbox.cpp
        src/sandbox/sandbox.h
        src/empowerment/empowerment.cpp
        src/empowerment/empowerment.h
        src/bomb/bomb.cpp
        src/bomb/bomb.h
)

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE sandbox)
target_link_libraries(${PROJECT_NAME} PUBLIC cap)
target_link_libraries(${PROJECT_NAME} PUBLIC seccomp)
