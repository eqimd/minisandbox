#include "sandbox/sandbox.h"

int main(int argc, char* argv[]) {
    struct sandbox_data data = {};
    data.executable_path = argv[1];
    data.perm_flags = 0;
    data.ram_limit_bytes = 0;
    data.time_execution_limit_ms = 0;

    run_sandbox(data);
}