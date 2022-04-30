#include <filesystem>
#include <stdint.h>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

struct sandbox_data {
    fs::path executable_path;
    fs::path rootfs_path;
    int perm_flags;
    milliseconds time_execution_limit_ms;
    bytes ram_limit_bytes;
};

void run_sandbox(const struct sandbox_data& data);