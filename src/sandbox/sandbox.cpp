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

#include "sandbox.h"


constexpr char* PUT_OLD = ".put_old";
constexpr char* MINISANDBOX_EXEC = ".minisandbox_exec";

namespace minisandbox {

struct clone_data {
    const char* executable;
    char** argv;
    char** envp;
};

void drop_privileges() {
    errno = 0;
    if (setgid(getgid()) == -1) {
        throw std::runtime_error(
            "Could not drop privileges (setgid or getgid): " +
            std::string(strerror(errno))
        );
    }
    errno = 0;
    if (setuid(getuid()) == -1) {
        throw std::runtime_error(
            "Could not drop privileges (setuid or getuid): " +
            std::string(strerror(errno))
        );
    }
}

int enter_pivot_root(void* arg) {
    //Allow tracing
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
        exit(EXIT_FAILURE);
    }
    // Stop n wait for tracer reaction
    if (raise(SIGSTOP)) {
        exit(EXIT_FAILURE);
    }

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
        throw std::runtime_error(
            "Could not unmount old root: " +
            std::string(strerror(errno))
        );
    }
    fs::remove(PUT_OLD);

    struct clone_data* data = (struct clone_data*)(arg);

    errno = 0;
    if (execvpe(data->executable, data->argv, data->envp) == -1) {
        throw std::runtime_error(
            "Could not execute " +
            std::string(data->executable) + ": " +
            std::string(strerror(errno))
        );
    }

    return 0;
}

sandbox::sandbox(const sandbox_data& sb_data) {
    executable_path = fs::absolute(sb_data.executable_path);
    rootfs_path = sb_data.rootfs_path;
    argv = sb_data.argv;
    envp = sb_data.envp;
    perm_flags = sb_data.perm_flags;

    rlimits.push_back({RLIMIT_AS, {sb_data.ram_limit_bytes, sb_data.ram_limit_bytes}});
    rlimits.push_back({RLIMIT_CPU, {sb_data.time_execution_limit_ms / 1000, sb_data.time_execution_limit_ms / 1000}}); //potentially zero

    stack_size = sb_data.stack_size;
}

sandbox::~sandbox() {
    try {
        clean_after_run();
    } catch (...) {}
}

void sandbox::run() {
    // TODO: run only elf???
    
    if (!fs::exists(executable_path)) {
        throw std::runtime_error(
            "Executable " +
            executable_path.string() +
            " does not exist."
        );
    }

    if (!fs::exists(rootfs_path)) {
        throw std::runtime_error(
            "Rootfs directory " +
            rootfs_path.string() +
            " does not exist."
        );
    }
    if (!fs::is_directory(rootfs_path)) {
        throw std::runtime_error(
            "Rootfs path " +
            rootfs_path.string() +
            " is not a directory."
        );
    }

    if (stack_size == 0) {
        throw std::runtime_error(
            "Stack size should not be zero."
        );
    }

    clone_stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void* stack_top = reinterpret_cast<char*>(clone_stack) + stack_size;

    bind_new_root(rootfs_path.c_str());
    fs::current_path(rootfs_path);

    fs::create_directory(MINISANDBOX_EXEC);

    fs::path exec_in_sandbox = fs::path(MINISANDBOX_EXEC) / executable_path.filename();
    fs::copy_file(executable_path, exec_in_sandbox);

    mount_to_new_root(NULL, "/", MS_PRIVATE | MS_REC);

    std::vector<const char*> argv_cstr;
    std::transform(
        argv.begin(),
        argv.end(),
        std::back_inserter(argv_cstr),
        [](const std::string& s) {
            return s.c_str();
        }
    );
    std::vector<const char*> envp_cstr;
    std::transform(
        envp.begin(),
        envp.end(),
        std::back_inserter(envp_cstr),
        [](const std::string& s) {
            return s.c_str();
        }
    );

    struct clone_data data = {};
    data.executable = exec_in_sandbox.c_str();
    data.argv = const_cast<char**>(argv_cstr.data());
    data.envp = const_cast<char**>(envp_cstr.data());

    errno = 0;
    child_pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWPID | SIGCHLD,
        &data
    );
    if (child_pid == -1) {
        throw std::runtime_error(
            "Could not clone process: " +
            std::string(strerror(errno))
        );
    }

    set_rlimits();
    int statloc;
    if (waitpid(child_pid, &statloc, 0) < 0) {
        throw std::runtime_error("Error while waiting for ptraceme from child");
    }
    if (!WIFSTOPPED(statloc) || WSTOPSIG(statloc) != SIGSTOP) {
        throw std::runtime_error("Unexpected returned status from child");
    }

    ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEEXIT);
    auto now_time = std::chrono::steady_clock::now();
    int ret = 0;
    // TODO: refactor me
    while (!WIFEXITED(statloc)) {
        ptrace(PTRACE_SYSCALL, child_pid, 0, 0);
        ret = waitpid(child_pid, &statloc, 0);
        if (WIFSTOPPED(statloc) && WSTOPSIG(statloc) & (1u << 7)) {
            //do smth
        } else {
            if (WSTOPSIG(statloc) == 11) {
                throw std::runtime_error("Segfault was thrown");
            }
            continue;
        }

        while (!WIFEXITED(statloc)) {
            ptrace(PTRACE_SYSCALL, child_pid, 0, 0);
            waitpid(child_pid, &statloc, 0);

            if (WIFSTOPPED(statloc) && WSTOPSIG(statloc) & (1u << 7)) {
                break;
            } 
        }
    }

    clean_after_run();
}

void sandbox::mount_to_new_root(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(
            "Could not mount " +
            std::string(mount_from) + ": " +
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
        munmap(clone_stack, stack_size);
        clone_stack = nullptr;
    }
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