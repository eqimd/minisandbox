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
    void execute();
    int get_pid();

private:
    void start_ram_monitoring();
    void start_time_monitoring();
};

};