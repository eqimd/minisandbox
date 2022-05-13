# compile c++
make -C ../.. -q
g++ -o wait wait.cpp


# start infinite program
./wait &
pid=$!

# make killer
echo "kill $pid;sleep 3s;" > main
sudo chmod 777 main

# try to kill
sudo ../../minisandbox  ./main ../../rootfs

# clear folder
kill $pid
rm wait
rm main


