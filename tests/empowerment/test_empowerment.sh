#!/bin/bash
# run it with sudo

g++ -std=c++17 test_empowerment.cpp ../../src/empowerment/empowerment.cpp -o test_empowerment -lcap 
g++ -std=c++17 exec.cpp -o exec1
g++ -std=c++17 exec_exec.cpp -o exec_exec

setcap cap_net_raw=epi exec1

cp exec_exec exec_exec_copy1
cp exec_exec exec_exec_copy2
cp exec_exec exec_exec_copy3
cp exec_exec exec_exec_copy4

setcap cap_net_raw=ep exec_exec_copy1
setcap cap_net_raw=ep exec_exec_copy2
setcap cap_net_raw=-ep exec_exec_copy2
chmod 744 exec_exec_copy4
chmod u+s exec_exec_copy4

# run tests
bash tests_scenarios.sh

# clear
rm exec1 test_empowerment exec_exec exec_exec_*

