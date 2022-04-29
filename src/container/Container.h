#include <filesystem>
#include <stdint.h>

namespace fs = std::filesystem;

namespace sandbox {

using milliseconds = uint64_t;
using bytes = uint64_t;

class Container {
public:
    Container(
        const fs::path& path_to_binary,
        int perm_flags,
        milliseconds time_execution_limit_ms,
        bytes ram_limit_bytes
    );
    void run();
    int get_pid();

private:
    fs::path _path;
    int _flags;
    milliseconds _time_exec;
    bytes _ram_limit;

    void mount_to_newroot(const char* mount_from, const char* mount_to, int flags);
    void bind_new_root();
    void unmount(const char* path);
};

};