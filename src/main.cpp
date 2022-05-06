#include "sandbox/sandbox.h"

int main(int argc, char* argv[]) {
    fs::path executable_path = argv[1];
    fs::path rootfs_path = argv[2];
    int perm_flags = 0;
    int ram_limit_bytes = 0;
    int stack_size = 10240;
    int time_execution_limit_ms = 0;

    minisandbox::sandbox sb(
        executable_path,
        rootfs_path,
        perm_flags,
        time_execution_limit_ms,
        ram_limit_bytes,
        stack_size
    );

    sb.run();
}