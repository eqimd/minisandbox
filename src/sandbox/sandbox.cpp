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
    fs::remove(PUT_OLD);

    fs::path exec_path = fs::absolute(reinterpret_cast<char*>(arg));

    char* argv[1];
    char* env[1];

    errno = 0;
    if (execvpe(exec_path.c_str(), argv, env) == -1) {
        throw std::runtime_error(
            "Can not execute " +
            exec_path.string() + ": " +
            std::string(strerror(errno))
        );
    }

    return 0;
}

sandbox::sandbox(fs::path executable_path, fs::path rootfs_path, int perm_flags, rlimits_arguments ra)
:   executable_path(fs::absolute(executable_path)),
    rootfs_path(fs::absolute(rootfs_path)),
    perm_flags(perm_flags),
    ra(ra),
    stack(ra.stack_size),
    edir(rootfs_path) {}

void sandbox::run() {
    // Add checks: executable exists, it is ELF
    // Add checks: root fs directory exists
    edir.copy_executable(executable_path);

    errno = 0;
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    errno = 0;
    pid_t pid = clone(
        enter_pivot_root,
        stack.get_stack_top(),
        CLONE_NEWNS | CLONE_NEWPID | SIGCHLD,
        edir.get_exec_path_data()
    );
    if (pid == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    int statloc;
    while (waitpid(pid, &statloc, 0) > 0) {}
}

void sandbox::mount_to_new_root(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

sandbox::sandbox_stack::sandbox_stack(size_t stack_size) : stack_size(stack_size) {
    stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        throw std::runtime_error("Something went wrong with mmap in stack initialization");
    }
    stack_top = reinterpret_cast<char *>(stack) + stack_size;
}

sandbox::sandbox_stack::~sandbox_stack() {
    munmap(stack, stack_size);
}

void *sandbox::sandbox_stack::get_stack_top() const {
    return stack_top;
}

sandbox::sandbox_exec_dir::sandbox_exec_dir(const fs::path &newroot) : root(newroot) {
    bind_new_root();
    fs::current_path(root);
    fs::create_directory(sandbox_dir);
}

sandbox::sandbox_exec_dir::~sandbox_exec_dir() {
    unmount(MNT_DETACH);
    fs::remove_all(sandbox_dir);
}

void sandbox::sandbox_exec_dir::copy_executable(const fs::path &p) {
    fs::copy_file(p, sandbox_dir / p.filename());
    exec = sandbox_dir / p.filename();
}

void *sandbox::sandbox_exec_dir::get_exec_path_data() {
    return reinterpret_cast<void*>(
            const_cast<char*>((exec.c_str()))
    );
}

void sandbox::sandbox_exec_dir::unmount(int flags) {
    fs::path current = ".";
    errno = 0;
    if (umount2(current.c_str(), flags) == -1) {
        throw std::runtime_error(
                "Can not unmount " +
                std::string(current.c_str()) + ": " +
                std::string(strerror(errno))
        );
    }
}

void sandbox::sandbox_exec_dir::bind_new_root() {
    errno = 0;
    if (mount(root.c_str(), root.c_str(), NULL, MS_BIND | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

}