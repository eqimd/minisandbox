#!/bin/bash

# init
bash init.sh

# build sandbox
bash build.sh

cd build/tests

# rlimits
echo rlimits
./rlimits/rlimits_test /

# bomb
echo bomb
./bomb/bomb_test

# empowerment
cd ../../tests/empowerment
echo empowerment
bash ./test_empowerment.sh
cd ..

# ...
