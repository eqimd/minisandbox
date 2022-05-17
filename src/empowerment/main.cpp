#include <iostream>
#include <unistd.h>

int main() {
    uid_t ruid, euid, suid;
    errno = 0;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("getresuid failed");
        return false;
    }
    std::cout << "main " << ruid << " " << euid << " " << suid << "\n";
    return 0;
}
