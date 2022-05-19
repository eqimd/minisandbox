#include <iostream>
#include <unistd.h>
#include <cassert>

int main(int argc, char *argv[]) {
    assert(argc == 2);

    uid_t ruid, euid, suid;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("getresuid failed");
        return -1;
    }
    assert (ruid == euid && euid == suid && suid != 0);

    char* argv_[] = {NULL};
    char* envp[] = {NULL};
    if (execve(argv[1], argv_, envp) == -1)
        return 1;
    return 0;
}
