#include <filesystem>
#include <stdint.h>
#include <sys/resource.h>
#include <vector>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

namespace minisandbox {

struct sandbox_rlimit {
    __rlimit_resource resource;
    rlimit rlim;
};

struct rlimits_arguments {
    sandbox_rlimit time_execution_limit;
    sandbox_rlimit ram_limit;
    bytes stack_size;
};

class sandbox {
public:
    sandbox(
        fs::path executable_path,
        fs::path rootfs_path,
        int perm_flags,
        rlimits_arguments ra
    );

    ~sandbox();

    void run();

private:
    void set_rlimits();

    class sandbox_stack {
    public:
        explicit sandbox_stack(size_t stack_size);
        ~sandbox_stack();

        void* get_stack_top() const;
    private:
        void* stack;
        void* stack_top;
        size_t stack_size;
    };

    class sandbox_exec_dir {
    public:
        explicit sandbox_exec_dir(const fs::path &newroot);
        ~sandbox_exec_dir();

        void bind_new_root();
        void unmount(int flags);

        void copy_executable(const fs::path &p);
        void *get_exec_path_data();
    private:
        const fs::path sandbox_dir = ".minisandbox_exec";
        fs::path root;
        fs::path exec;
    };

    fs::path executable_path;
    fs::path rootfs_path;
    int perm_flags;
    rlimits_arguments ra;

    sandbox_stack stack;
    sandbox_exec_dir edir;
    std::vector<sandbox_rlimit> rls;

    pid_t child_pid;
    
    void mount_to_new_root(const char* mount_from, const char* mount_to, int flags);
};

}
