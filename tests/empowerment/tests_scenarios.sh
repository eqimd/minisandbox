#!/bin/bash
. assert.sh

assert_raises "./test_empowerment ./exec_exec_copy1" "0"
assert_raises "./test_empowerment ./exec_exec_copy2" "0"
assert_raises "./test_empowerment ./exec_exec_copy3" "0"
assert_raises "./test_empowerment ./exec_exec_copy4" "1"

# check files after
assert "getcap exec_exec_copy1" "exec_exec_copy1 ="
assert "getcap exec_exec_copy2" "exec_exec_copy2 ="
assert "getcap exec_exec_copy3" "exec_exec_copy3 ="
assert "getcap exec_exec_copy4" "exec_exec_copy4 ="

assert_end examples
