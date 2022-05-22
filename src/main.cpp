#include "sandbox/sandbox.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "bomb/bomb.h"

constexpr int RAW_LIMIT = 100000000;
constexpr int STACK_SIZE = 100000000;
constexpr int TIME_LIMIT_MS = 1000;
constexpr int FORK_LIMIT = 5;


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

        std::string argv_config = config.value("argv", "");
        std::string envp_config = config.value("envp", "");
        boost::split(data.argv, argv_config, boost::is_any_of(" "));
        boost::split(data.envp, envp_config, boost::is_any_of(" "));

        data.ram_limit_bytes = config.value("ram_limit", RAW_LIMIT);
        data.stack_size = config.value("stack_size", STACK_SIZE);
        data.time_execution_limit_ms = config.value("time_limit", TIME_LIMIT_MS);
        data.fork_limit = config.value("fork_limit", FORK_LIMIT);

        minisandbox::sandbox sb(data);

        sb.run();
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}