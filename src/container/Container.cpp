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

#include "Container.h"

constexpr char* NEW_ROOT = "newroot";
constexpr char* PUT_OLD = "put_old";


namespace sandbox {

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

    // drop_privileges();

    fs::create_directories(PUT_OLD);
    
    errno = 0;
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    
    errno = 0;
    if (syscall(SYS_pivot_root, ".", PUT_OLD) == -1) {
        std::cout << "pivot_root " << errno << std::endl;
        throw std::runtime_error(std::string(strerror(errno)));
    }
    
    fs::current_path("/");

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

Container::Container(
    const fs::path& path_to_binary,
    int perm_flags,
    milliseconds time_execution_limit_ms,
    bytes ram_limit_bytes
) {
    _path = path_to_binary;
    _flags = perm_flags;
    _time_exec = time_execution_limit_ms;
    _ram_limit = ram_limit_bytes;
}

void Container::run() {
    // TODO: Add stack size to constructor
    int STACK_SIZE = 10240;
    void* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void* stack_top = stack + STACK_SIZE;

    std::string binpath = fs::absolute(_path).string();

    bind_new_root();
    fs::current_path(NEW_ROOT);

    fs::create_directories("lib");
    mount_to_newroot("/lib", "lib", MS_BIND | MS_REC);

    fs::create_directories("lib64");
    mount_to_newroot("/lib64", "lib64", MS_BIND | MS_REC);

    fs::create_directories("usr/lib");
    mount_to_newroot("/usr/lib", "usr/lib", MS_BIND | MS_REC);
    
    fs::create_directories("usr/lib64");
    mount_to_newroot("/usr/lib64", "usr/lib64", MS_BIND | MS_REC);

    std::ofstream bin_to_run(_path.filename());
    // std::cout << binpath << " " << _path.filename() << std::endl;
    mount_to_newroot(binpath.c_str(), _path.filename().c_str(), MS_BIND | MS_REC | MS_RDONLY);
    bin_to_run.close();

    errno = 0;
    pid_t pid = clone(enter_pivot_root, stack_top, CLONE_NEWNS | CLONE_NEWPID | SIGCHLD, (void*)(binpath.c_str()));
    if (pid == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    int statloc;
    while (waitpid(pid, &statloc, 0) > 0) {}
    
    errno = 0;
    if (umount2(".", MNT_DETACH) == -1) {
        std::cerr << "Could not unmount new root." << std::endl;
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

void Container::mount_to_newroot(const char* mount_from, const char* mount_to, int flags) {
    errno = 0;
    if (mount(mount_from, mount_to, NULL, flags, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

void Container::bind_new_root() {
    fs::create_directory(NEW_ROOT);

    errno = 0;
    if (mount(NEW_ROOT, NEW_ROOT, NULL, MS_BIND | MS_REC, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

};