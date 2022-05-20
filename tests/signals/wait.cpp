#include <unistd.h>
#include <iostream>

int main() {
    while(true) {
        std::cout << "doing something\n";
        usleep(1000000);
    }
    return 0;
}
