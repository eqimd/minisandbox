#include <unistd.h>
#include <iostream>

int main() {
    fork();
    while(true) {
        std::cout << "Pid: " << getpid() << " doing something\n";
        usleep(1000000);
    }
    return 0;
}