cmake_minimum_required(VERSION 3.11)

project(minisandbox)

set(CMAKE_CXX_STANDARD 20)

include_directories("src")

set(sandbox_src
        ${${PROJECT_NAME}_SOURCE_DIR}/src/sandbox/sandbox.cpp
        ${${PROJECT_NAME}_SOURCE_DIR}/src/sandbox/sandbox.h
        ${${PROJECT_NAME}_SOURCE_DIR}/src/empowerment/empowerment.cpp
        ${${PROJECT_NAME}_SOURCE_DIR}/src/empowerment/empowerment.h
        ${${PROJECT_NAME}_SOURCE_DIR}/src/bomb/bomb.cpp
        ${${PROJECT_NAME}_SOURCE_DIR}/src/bomb/bomb.h
)

find_package(GTest REQUIRED)
add_subdirectory(tests)

add_library(
        sandbox
        SHARED
        ${sandbox_src}
)

add_executable(
        ${PROJECT_NAME}
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE sandbox)
target_link_libraries(sandbox PUBLIC cap)
target_link_libraries(sandbox PUBLIC seccomp)
