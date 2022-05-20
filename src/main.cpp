#include "sandbox/sandbox.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

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

        std::string argv_config = config["argv"];
        std::string envp_config = config["envp"];
        boost::split(data.argv, argv_config, boost::is_any_of(" "));
        boost::split(data.envp, envp_config, boost::is_any_of(" "));

        data.perm_flags = config["perm_flags"];
        data.ram_limit_bytes = config["ram_limit"];
        data.stack_size = config["stack_size"];
        data.time_execution_limit_ms = config["time_limit"];

        data.priority = config["priority"];
        data.io_priority = config["io_priority"];

        minisandbox::sandbox sb(data);

        sb.run();
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}