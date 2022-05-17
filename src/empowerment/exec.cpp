#include <iostream>
#include <unistd.h>
#include <cassert>
#include <string>

int main(int argc, char *argv[]) {
    uid_t ruid, euid, suid;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("getresuid failed");
        return false;
    }
    std::cout << "exec " << ruid << " " << euid << " " << suid << "\n";

    assert(argc == 2);
    char* argv_[] = {NULL};
    char* envp[] = {NULL};
    if (execve(argv[1], argv_, envp) == -1)
        perror(("Could not execve" + std::string(argv[1])).c_str());
    return 0;
}
