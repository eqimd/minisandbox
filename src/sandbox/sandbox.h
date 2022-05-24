#include <filesystem>
#include <stdint.h>
#include <vector>
#include <string>
#include <sys/resource.h>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

constexpr int PRIORITY_DEFAULT = 0;
constexpr int IO_PRIORITY_DEFAULT = 0;

constexpr int STACK_DEFAULT_VALUE = 1024 * 1024 * 4;    // 4 mb

constexpr int RAM_VALUE_NO_LIMIT = 0;
constexpr int TIME_VALUE_NO_LIMIT = 0;
constexpr int FORK_VALUE_NO_LIMIT = -1;

constexpr char* PUT_OLD = ".put_old";
constexpr char* MINISANDBOX_EXEC = ".minisandbox_exec";

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
    milliseconds time_execution_limit_s;
    bytes ram_limit_bytes;
    int priority = PRIORITY_DEFAULT;
    int io_priority = IO_PRIORITY_DEFAULT;
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
    void set_priority();

    pid_t child_pid;

    std::string old_path;
};

};
