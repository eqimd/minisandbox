#include "sandbox/sandbox.h"

int main(int argc, char* argv[]) {
    fs::path executable_path = argv[1];
    fs::path rootfs_path = argv[2];
    int perm_flags = 0;
    bytes stack_size = 10240;

    minisandbox::sandbox_rlimit time_execution_limit;
    minisandbox::sandbox_rlimit ram_limit;

    rlimit r;
    getrlimit(RLIMIT_AS, &r);
    ram_limit = {
            .resource = RLIMIT_AS,
            .rlim = {
                    .rlim_cur = 102400000,
                    .rlim_max = r.rlim_max,
            }
    };


    getrlimit(RLIMIT_CPU, &r);
    time_execution_limit = {
            .resource = RLIMIT_CPU,
            .rlim = {
                    .rlim_cur = 2,
                    .rlim_max = r.rlim_max,
            }
    };

    minisandbox::sandbox sb(
        executable_path,
        rootfs_path,
        perm_flags,
        { time_execution_limit,
        ram_limit,
        stack_size }
    );

    sb.run();
}