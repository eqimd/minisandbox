#include <iostream>
#include <unistd.h>
#include <cassert>
#include "../../src/empowerment/empowerment.h"


int main(int argc, char *argv[]) {
    assert(argc == 2);
    char exec[] = "exec1";
    char* exec_exec = argv[1];
    char* argv_[] = {exec, exec_exec, NULL};
    char* envp[] = {NULL};

    assert(minisandbox::empowerment::set_capabilities(exec_exec));
    minisandbox::empowerment::drop_privileges();

    return execve(exec, argv_, envp);
    return 0;
}
