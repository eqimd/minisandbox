#include <iostream>
#include <unistd.h>
#include <cassert>

int main() {
    uid_t ruid, euid, suid;
    errno = 0;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("getresuid failed");
        return 1;
    }
    assert (ruid == euid && euid == suid && suid != 0);
    return 0;
}
