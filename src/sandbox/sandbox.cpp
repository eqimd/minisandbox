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

#include "sandbox.h"


constexpr char* PUT_OLD = ".put_old";


namespace minisandbox {

void drop_privileges() {
    errno = 0;
    if (setgid(getgid()) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    errno = 0;
    if (setuid(getuid()) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

int enter_pivot_root(void* arg) {
    drop_privileges();

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
        throw std::runtime_error(std::string(strerror(errno)));
    }

    std::string exec_path = "/" + std::string(reinterpret_cast<char*>(arg));

    char* argv[1];
    char* env[1];

    errno = 0;
    if (execvpe(exec_path.c_str(), argv, env) == -1) {
        throw std::runtime_error(
            "Can not execute " +
            exec_path + ": " +
            std::string(strerror(errno))
        );
    }

    return 0;
}

sandbox::sandbox(
    fs::path executable_path,
    fs::path rootfs_path,
    int perm_flags,
    milliseconds time_execution_limit_ms,
    bytes ram_limit_bytes,
    bytes stack_size
) : executable_path(executable_path),
    rootfs_path(rootfs_path),
    perm_flags(perm_flags),
    time_execution_limit_ms(time_execution_limit_ms),
    ram_limit_bytes(ram_limit_bytes),
    stack_size(stack_size) {}

void sandbox::run() {
    // Add checks: executable exists, it is ELF
    // Add checks: root fs directory exists

    fs::copy_file(executable_path, rootfs_path / executable_path);

    void* stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void* stack_top = stack + stack_size;

    bind_new_root(rootfs_path.c_str());
    fs::current_path(rootfs_path);

    errno = 0;
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    errno = 0;
    void* exec_path_casted = reinterpret_cast<void*>(
        const_cast<char*>((executable_path.filename().c_str()))
    );
    pid_t pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWPID | SIGCHLD,
        exec_path_casted
    );
    if (pid == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    int statloc;
    while (waitpid(pid, &statloc, 0) > 0) {}

    unmount(".", MNT_DETACH);
    fs::remove(executable_path.filename());
    munmap(stack, stack_size);
}

void sandbox::mount_to_new_root(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

void sandbox::unmount(const char* path, int flags) {
    errno = 0;
    if (umount2(path, flags) == -1) {
        throw std::runtime_error(
            "Can not unmount " +
            std::string(path) + ": " +
            std::string(strerror(errno))
        );
    }
}

void sandbox::bind_new_root(const char* new_root) {
    errno = 0;
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

};