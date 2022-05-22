add_executable(bomb_test test_bomb.cpp)

target_link_libraries(bomb_test ${test_libs} sandbox)
