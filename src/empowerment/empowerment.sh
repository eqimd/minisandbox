#!/bin/bash
# run empowerment.sh with sudo

g++ -std=c++17 empowerment.cpp -o empowerment -lcap 
g++ -std=c++17 exec.cpp -o exec1
g++ -std=c++17 main.cpp -o main

setcap CAP_SETFCAP+epi empowerment

chmod u+s empowerment

# chmod 744 exec1
# chmod u+s exec1
setcap cap_net_raw=epi exec1

cp main main_copy1
cp main main_copy2
cp main main_copy3
cp main main_copy4

setcap cap_net_raw=ep main_copy1
setcap cap_net_raw=ep main_copy2
setcap cap_net_raw=-ep main_copy2
chmod 744 main_copy4
chmod u+s main_copy4
