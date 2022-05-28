set(test_libs
    ${GTEST_LIBRARIES}     
    pthread
)

add_subdirectory(rlimits)
add_subdirectory(bomb)
