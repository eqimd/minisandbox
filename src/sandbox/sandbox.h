#include <filesystem>
#include <stdint.h>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

namespace minisandbox {

struct sandbox_data {
    fs::path executable_path;
    fs::path rootfs_path;
    int perm_flags;
    milliseconds time_execution_limit_ms;
    bytes ram_limit_bytes;
    bytes stack_size;
};

class sandbox {
public:
    sandbox(
        fs::path executable_path,
        fs::path rootfs_path,
        int perm_flags,
        milliseconds time_execution_limit_ms,
        bytes ram_limit_bytes,
        bytes stack_size
    );
    sandbox(const sandbox_data& sb_data);
    ~sandbox();

    void run();

private:
    fs::path executable_path;
    fs::path rootfs_path;
    int perm_flags;
    milliseconds time_execution_limit_ms;
    bytes ram_limit_bytes;
    bytes stack_size;

    void* clone_stack = nullptr;
    
    void mount_to_new_root(const char* mount_from, const char* mount_to, int flags);
    void unmount(const char* path, int flags);
    void bind_new_root(const char* new_root);
    void clean_after_run();
};

};
