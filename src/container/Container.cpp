#include <sched.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <iostream>

#include "Container.h"


namespace sandbox {

int run_cloned(void* arg) {
    char* binpath = reinterpret_cast<char*>(arg);

    char* new_root = "newroot";
    char* put_old = "newroot/oldroot";
    fs::create_directories(put_old);

    errno = 0;
    if (mount(new_root, new_root, NULL, MS_BIND, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    fs::current_path("new_root");
    
    errno = 0;
    if (syscall(SYS_pivot_root, ".", "oldroot") == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    
    fs::current_path("/");

    errno = 0;
    if (mount("/oldroot/lib", "/lib", "ext4", 0, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    errno = 0;
    if (mount("/oldroot/lib64", "/lib64", "ext4", 0, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    errno = 0;
    if (mount("/oldroot/usr/lib", "/usr/lib", "ext4", 0, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    errno = 0;
    if (mount("/oldroot/usr/lib64", "/usr/lib64", "ext4", 0, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
    errno = 0;
    fs::path path_to_bin = fs::path("/oldroot") / fs::path(binpath);
    if (mount(path_to_bin.c_str(), path_to_bin.filename().c_str(), NULL, MS_BIND, NULL) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    errno = 0;
    if (umount2(put_old, MNT_DETACH) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }

    errno = 0;
    char* argv[1];
    if (execv(path_to_bin.filename().c_str(), argv) == -1) {
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
    int STACK_SIZE = 4096;
    void* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    void* stack_top = stack + STACK_SIZE;

    void* binpath_void = reinterpret_cast<void*>(const_cast<char*>((_path.parent_path() / _path.filename()).string().c_str()));

    errno = 0;
    if (clone(run_cloned, stack_top, CLONE_NEWNS | CLONE_IO, binpath_void) == -1) {
        throw std::runtime_error(std::string(strerror(errno)));
    }
}

};