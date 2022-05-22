#include <filesystem>
#include <stdint.h>
#include <vector>
#include <string>
#include <sys/resource.h>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

namespace minisandbox {

struct sandbox_rlimit {
    __rlimit_resource resource;
    rlimit rlim;
};

struct sandbox_data {
    fs::path executable_path;
    fs::path rootfs_path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
    bytes stack_size;
    milliseconds time_execution_limit_ms;
    bytes ram_limit_bytes;
    int fork_limit;
};

class sandbox {
public:
    sandbox(const sandbox_data& sb_data);
    ~sandbox();

    void run();

private:
    sandbox_data _data = {};
    std::vector<sandbox_rlimit> rlimits;

    void* clone_stack = nullptr;
    
    void mount_to_new_root(const char* mount_from, const char* mount_to, int flags);
    void unmount(const char* path, int flags);
    void bind_new_root(const char* new_root);
    void set_rlimits();
    void clean_after_run();

    pid_t child_pid;

    std::string old_path;
};

};
