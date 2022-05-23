#include "sandbox/sandbox.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

#include "bomb/bomb.h"


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./" << argv[0] << " <config path>" << std::endl;
        return 0;
    }


    try {
        nlohmann::json config;
        std::ifstream config_file(argv[1]);
        config_file >> config;

        minisandbox::sandbox_data data = {};
        data.executable_path = std::string(config["executable"]);
        data.rootfs_path = std::string(config["rootfs"]);

        auto argv = config.value("argv", nlohmann::basic_json<>());
        auto envp = config.value("envp", nlohmann::basic_json<>());
        data.argv.insert(data.argv.end(), argv.begin(), argv.end());
        data.envp.insert(data.envp.end(), envp.begin(), envp.end());

        data.ram_limit_bytes = config.value("ram_limit", RAM_VALUE_NO_LIMIT);
        data.stack_size = config.value("stack_size", STACK_DEFAULT_VALUE);
        data.time_execution_limit_ms = config.value("time_limit", TIME_VALUE_NO_LIMIT);
        data.fork_limit = config.value("fork_limit", FORK_VALUE_NO_LIMIT);

        data.priority = config.value("priority", PRIORITY_DEFAULT);
        data.io_priority = config.value("io_priority", IO_PRIORITY_DEFAULT);

        minisandbox::sandbox sb(data);

        sb.run();
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}