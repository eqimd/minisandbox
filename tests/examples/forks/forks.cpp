#include <iostream>
#include <unistd.h>
#include <cassert>


void tracee_rec(int forks_remaining, int sleep_time = 1) {
    if (forks_remaining == 0) {
        sleep(sleep_time);
        return;
    }
    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("fork failed: too many forks");
    }
    if (pid != 0) {
        std::cout << "fork " << forks_remaining << std::endl;
        sleep(sleep_time);
        exit(0);
    } else {
        tracee_rec(forks_remaining - 1);
    }
}


int main(int argc, const char *argv[]) {
    assert(argc <= 2);
    int forks_remaining = 4;
    if (argc == 2) {
        forks_remaining = std::stoi(argv[1]);
    }
    pid_t pid = fork();
    if (pid != 0) {
        sleep(2);
        return 0;
    }
    tracee_rec(forks_remaining);
    exit(0);
}
