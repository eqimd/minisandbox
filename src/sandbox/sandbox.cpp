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


void mount_to_new_root(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

void bind_new_root(const char* new_root) {
    errno = 0;
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

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
    char* binpath = reinterpret_cast<char*>(arg);
    // std::cout << binpath << std::endl;

    drop_privileges();

    fs::create_directories(PUT_OLD);
    
    errno = 0;
    if (syscall(SYS_pivot_root, ".", PUT_OLD) == -1) {
        std::cout << "pivot_root " << errno << std::endl;
        throw std::runtime_error(std::string(strerror(errno)));
    }
    
    fs::current_path("/");

    // make path /put_old/<path_to_bin>
    char fullpath_bin[100];
    fullpath_bin[0] = '/';
    strcat(fullpath_bin, PUT_OLD);
    strcat(fullpath_bin, binpath);

    // if (mount(path_to_bin.c_str(), path_to_bin.filename().c_str(), NULL, MS_BIND, NULL) == -1) {
    //     throw std::runtime_error(std::string(strerror(errno)));
    // }

    // errno = 0;
    // if (umount2(PUT_OLD, MNT_DETACH) == -1) {
    //     throw std::runtime_error(std::string(strerror(errno)));
    // }
    // std::cout << "here" << std::endl;

    errno = 0;
    char* argv[1];
    char* env[1];
    if (execvpe(fullpath_bin, argv, env) == -1) {
        std::cout << "exec " << errno << std::endl;
        throw std::runtime_error(std::string(strerror(errno)));
    }

    return 0;
}

void unmount(const char* path, int flags) {
    errno = 0;
    if (umount2(path, flags) == -1) {
        throw std::runtime_error(
            "Could not unmount " +
            std::string(path) + "\n" +
            std::string(strerror(errno))
        );
    }
}

void run_sandbox(const struct sandbox_data& data) {
    // TODO: Add stack size to constructor
    void* stack = mmap(NULL, data.stack_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void* stack_top = stack + data.stack_size;

    std::string binpath = fs::absolute(data.executable_path).string();

    bind_new_root(data.rootfs_path.c_str());
    fs::current_path(data.rootfs_path);

    // std::ofstream bin_to_run(data.path_to_binary.filename());
    // std::cout << binpath << " " << _path.filename() << std::endl;
    // mount_to_newroot(binpath.c_str(), data.path_to_binary.filename().c_str(), MS_BIND | MS_REC | MS_RDONLY);
    // bin_to_run.close();

    errno = 0;
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    errno = 0;
    pid_t pid = clone(enter_pivot_root, stack_top, CLONE_NEWNS | CLONE_NEWPID | SIGCHLD, (void*)(binpath.c_str()));
    if (pid == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    int statloc;
    while (waitpid(pid, &statloc, 0) > 0) {}

    unmount(".", MNT_DETACH);
}
