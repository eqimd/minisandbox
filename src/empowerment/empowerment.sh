#!/bin/bash
g++ -std=c++17 empowerment.cpp -o empowerment -lcap 

cp empowerment empowerment_copy1
cp empowerment empowerment_copy2
cp empowerment empowerment_copy3
cp empowerment empowerment_copy4

sudo setcap cap_net_raw=ep empowerment_copy1
sudo setcap cap_net_raw=ep empowerment_copy2
sudo setcap cap_net_raw=-ep empowerment_copy2
chmod u+s empowerment_copy4

./empowerment empowerment_copy1 # 0
./empowerment empowerment_copy2 # 1
./empowerment empowerment_copy3 # 1
./empowerment empowerment_copy4 # 0
