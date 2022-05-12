#include <filesystem>
#include <stdint.h>

namespace fs = std::filesystem;

using milliseconds = uint64_t;
using bytes = uint64_t;

namespace minisandbox {

struct rlimits_arguments {
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
        rlimits_arguments ra
    );

    void run();

private:

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
        void *get_path_data();
    private:
        const fs::path sandbox_dir = ".minisandbox_exec";
        fs::path root;
    };

    fs::path executable_path;
    fs::path rootfs_path;
    int perm_flags;
    rlimits_arguments ra;

    sandbox_stack stack;
    sandbox_exec_dir edir;
    
    void mount_to_new_root(const char* mount_from, const char* mount_to, int flags);
};

}
