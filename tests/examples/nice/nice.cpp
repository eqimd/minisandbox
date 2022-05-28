#include <sys/resource.h>
#include <sys/syscall.h>
#include <asm/unistd.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <cstring>

#define IOPRIO_WHO_PGRP     (2)

int main() {
    errno = 0;
    int priority = getpriority(PRIO_PGRP, getpgid(getpid()));
    if (errno != 0) {
        throw std::runtime_error(
            "Could not set priority: " +
            std::string(strerror(errno))
        );
    }

    std::cout << "Priority is : " << priority << "\n";


    return 0;
}