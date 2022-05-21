add_subdirectory(mem)
add_subdirectory(time)

add_executable(rlimits_test
        ${sandbox_src}
        rlimits_test.cpp)
        
target_link_libraries(rlimits_test ${test_libs} sandbox)
