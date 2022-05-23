
#include <gtest/gtest.h>
#include "../../src/sandbox/sandbox.h"

namespace {
std::string rootfs;

size_t ram = 102400000;
size_t stack = 102400;
}

TEST(RlimitsTest, MemTest) {
    minisandbox::sandbox_data data = {};
    data.executable_path = std::string("mem/mem");
    data.rootfs_path = std::string(rootfs);

    data.ram_limit_bytes = ram;
    data.stack_size = stack;
    data.time_execution_limit_ms = 10000;

    minisandbox::sandbox sb(data);
    ASSERT_THROW(sb.run(), std::runtime_error);
}

TEST(RlimitsTest, TimeTest) {
    minisandbox::sandbox_data data = {};
    data.executable_path = std::string("time/time");
    data.rootfs_path = std::string(rootfs);

    data.ram_limit_bytes = ram;
    data.stack_size = stack;
    data.time_execution_limit_ms = 1000;

    minisandbox::sandbox sb(data);
    ASSERT_THROW(sb.run(), std::runtime_error);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Set rootfs to run tests" << std::endl;
        return EXIT_FAILURE;
    }

    testing::InitGoogleTest(&argc, argv);
    rootfs = argv[1];
    return RUN_ALL_TESTS();
}