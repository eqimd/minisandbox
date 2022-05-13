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
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <sys/ptrace.h>
#include <sys/param.h>
#include "sandbox.h"
#include "empowerment/empowerment.h"
#include "bomb/bomb.h"

#define IOPRIO_WHO_PGRP     (2)
#define IOPRIO_CLASS_RT     (1)
#define IOPRIO_CLASS_SHIFT	(13)
#define IOPRIO_PRIO_VALUE(class, data)	(((class) << IOPRIO_CLASS_SHIFT) | data)


namespace minisandbox {

void prepare_procfs()
{
    fs::create_directories("/proc");

    if (mount("proc", "/proc", "proc", 0, "")) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

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


    prepare_procfs();

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

    std::vector<char*> cargv;
    cargv.reserve(data->argv.size() + 1);
    std::transform(
        data->argv.begin(), 
        data->argv.end(),
        std::back_inserter(cargv),
        [](const std::string& s){
            return const_cast<char*>(s.c_str());
        }
    );
    cargv.push_back(NULL);

    std::vector<char*> cenvp;
    cenvp.reserve(data->envp.size());
    std::transform(
        data->envp.begin(), 
        data->envp.end(),
        std::back_inserter(cenvp),
        [](const std::string& s){
            return const_cast<char*>(s.c_str());
        }
    );

    errno = 0;
    if (execvpe(data->executable_path.c_str(), cargv.data(), cenvp.data()) == -1) {
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
    _data.rootfs_path = fs::absolute(_data.rootfs_path);
    _data.argv.insert(_data.argv.begin(), _data.executable_path.filename());

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
    set_priority();

    errno = 0;
    child_pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD,
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

void sandbox::set_priority() {
    if (_data.priority < -20 || _data.priority > 19) {
        throw std::runtime_error(
            "Wrong priority value, it should be in interval [-20, 19], but it is " +
            std::to_string( _data.priority)
        );
    }

    if (_data.io_priority < 0 || _data.io_priority > 7) {
        throw std::runtime_error(
            "Wrong io_priority value, it should be in interval [0, 7], but it is " +
            std::to_string( _data.io_priority)
        );
    }

    errno = 0;
    setpriority(PRIO_PGRP, getpgid(getpid()), _data.priority);
    if (errno != 0) {
        throw std::runtime_error(
            "Could not set priority: " +
            std::string(strerror(errno))
        );
    }

    errno = 0;
    if (syscall(SYS_ioprio_set, IOPRIO_WHO_PGRP, getpgid(getpid()), IOPRIO_PRIO_VALUE(IOPRIO_CLASS_RT, _data.io_priority)) == -1) {
        throw std::runtime_error(
            "Could not set io_priority: " +
            std::string(strerror(errno))
        );
    }
}

};
