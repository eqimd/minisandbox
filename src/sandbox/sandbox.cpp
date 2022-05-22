#include <sched.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <sys/ptrace.h>
#include <sys/param.h>
#include "sandbox.h"
#include "empowerment/empowerment.h"
#include "bomb/bomb.h"


namespace minisandbox {

int enter_pivot_root(void* arg) {
    fs::create_directories(PUT_OLD);
    
    errno = 0;
    if (syscall(SYS_pivot_root, ".", PUT_OLD) == -1) {
        throw std::runtime_error(
            "pivot_root is not succeeded: " +
            std::string(strerror(errno))
        );
    }    
    fs::current_path("/");

    errno = 0;
    if (umount2(PUT_OLD, MNT_DETACH) == -1) {
        throw std::runtime_error(
            "Could not unmount old root: " +
            std::string(strerror(errno))
        );
    }
    fs::remove(PUT_OLD);

    sandbox_data* data = reinterpret_cast<sandbox_data*>(arg);

    if (!minisandbox::empowerment::set_capabilities(data->executable_path.c_str())) {
        throw std::runtime_error("Could not set capabilities.");
    }

    minisandbox::empowerment::drop_privileges();
    
    if (data->fork_limit >= 0) {
        minisandbox::forkbomb::make_tracee();
    } else {
        // minisandbox::forkbomb::set_filters();
    } 

    std::vector<const char*> argv;
    std::transform(
        data->argv.begin(),
        data->argv.end(),
        std::back_inserter(argv),
        [](const std::string& s) {
            return s.c_str();
        }
    );
    std::vector<const char*> envp;
    std::transform(
        data->envp.begin(),
        data->envp.end(),
        std::back_inserter(envp),
        [](const std::string& s) {
            return s.c_str();
        }
    );

    char* const* argv_casted = const_cast<char**>(argv.data());
    char* const* envp_casted = const_cast<char**>(envp.data());

    errno = 0;
    if (execvpe(data->executable_path.c_str(), argv_casted, envp_casted) == -1) {
        throw std::runtime_error(
            "Could not execute " +
            data->executable_path.string() + ": " +
            std::string(strerror(errno))
        );
    }

    return 0;
}

sandbox::sandbox(const sandbox_data& sb_data) {
    _data = sb_data;
    _data.executable_path = fs::absolute(_data.executable_path);

    if (sb_data.ram_limit_bytes != RAM_VALUE_NO_LIMIT) {
        rlimits.push_back({RLIMIT_AS, {sb_data.ram_limit_bytes, sb_data.ram_limit_bytes}});
    }
    if (sb_data.time_execution_limit_ms != TIME_VALUE_NO_LIMIT) {
        rlimits.push_back({RLIMIT_CPU, {sb_data.time_execution_limit_ms / 1000, sb_data.time_execution_limit_ms / 1000}}); //potentially zero
    }
}

sandbox::~sandbox() {
    try {
        clean_after_run();
    } catch (...) {}
}

void sandbox::run() {
    // TODO: run only elf???
    
    if (!fs::exists(_data.executable_path)) {
        throw std::runtime_error(
            "Executable " +
            _data.executable_path.string() +
            " does not exist."
        );
    }

    if (!fs::exists(_data.rootfs_path)) {
        throw std::runtime_error(
            "Rootfs directory " +
            _data.rootfs_path.string() +
            " does not exist."
        );
    }
    if (!fs::is_directory(_data.rootfs_path)) {
        throw std::runtime_error(
            "Rootfs path " +
            _data.rootfs_path.string() +
            " is not a directory."
        );
    }

    if (_data.stack_size == 0) {
        throw std::runtime_error(
            "Stack size should not be zero."
        );
    }

    errno = 0;
    clone_stack = mmap(NULL, _data.stack_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);

    if (clone_stack == MAP_FAILED) {
        throw std::runtime_error(
            "Could not mmap " +
            std::to_string(_data.stack_size) +
            " bytes of memory for stack: " +
            std::string(strerror(errno))
        );
    }
    void* stack_top = reinterpret_cast<char*>(clone_stack) + _data.stack_size;

    bind_new_root(_data.rootfs_path.c_str());

    old_path = fs::current_path();
    fs::current_path(_data.rootfs_path);

    fs::create_directory(MINISANDBOX_EXEC);

    fs::path exec_in_sandbox = fs::path(MINISANDBOX_EXEC) / _data.executable_path.filename();
    fs::copy_file(_data.executable_path, exec_in_sandbox);
    _data.executable_path = exec_in_sandbox;

    mount_to_new_root(NULL, "/", MS_PRIVATE | MS_REC);

    errno = 0;
    child_pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWPID | SIGCHLD,
        reinterpret_cast<void*>(&_data)
    );
    if (child_pid == -1) {
        throw std::runtime_error(
            "Could not clone process: " +
            std::string(strerror(errno))
        );
    }
    
    set_rlimits();
    if (_data.fork_limit >= 0) {
        minisandbox::forkbomb::tracer(_data.fork_limit);
    } else {
        waitpid(child_pid, NULL, 0);
    }

    clean_after_run();
}

void sandbox::mount_to_new_root(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(
            "Could not mount " +
            std::string(mount_from) + 
            "to " + std::string(mount_to) + ": " +
            std::string(strerror(errno))
        );
    }
}

void sandbox::unmount(const char* path, int flags) {
    errno = 0;
    if (umount2(path, flags) == -1) {
        throw std::runtime_error(
            "ERROR: Could not unmount " +
            std::string(path) + ": " +
            std::string(strerror(errno))
        );
    }
}

void sandbox::bind_new_root(const char* new_root) {
    errno = 0;
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) == -1) {
        throw std::runtime_error(
            "Could not bind new root " +
            std::string(new_root) + ": " +
            std::string(strerror(errno))
        );
    }
}

void sandbox::clean_after_run() {
    unmount(".", MNT_DETACH);
    fs::remove_all(MINISANDBOX_EXEC);
    if (clone_stack != nullptr) {
        munmap(clone_stack, _data.stack_size);
        clone_stack = nullptr;
    }
    fs::current_path(old_path);
}

void sandbox::set_rlimits() {
    for (const sandbox_rlimit &r: rlimits) {
        errno = 0;
        int ret = prlimit(child_pid, r.resource, &r.rlim, NULL);
        if (ret < 0) {
            throw std::runtime_error("Error while setting rlimits: " + std::string(strerror(errno)));
        }
    }
}

};