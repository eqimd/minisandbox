#include <unistd.h>
#include <iostream>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        while(true) {
            std::cout << "doing something\n";
            usleep(1000000);
        }
    } else {
        while(true) {
            std::cout << "doing another something\n";
            usleep(1000000);
        }
    }
    return 0;
}