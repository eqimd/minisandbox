#include "sandbox/sandbox.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./" << argv[0] << " <config path>" << std::endl;
        return 0;
    }

    nlohmann::json config;
    std::ifstream config_file(argv[1]);
    config_file >> config;

    fs::path executable_path = config["executable"];
    fs::path rootfs_path = config["rootfs"];
    int perm_flags = config["perm_flags"];
    int ram_limit_bytes = config["ram_limit"];
    int stack_size = config["stack_size"];
    int time_execution_limit_ms = config["time_limit"];

    minisandbox::sandbox sb(
        executable_path,
        rootfs_path,
        perm_flags,
        time_execution_limit_ms,
        ram_limit_bytes,
        stack_size
    );

    sb.run();
}