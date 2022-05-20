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
#include <sys/param.h>
#include "sandbox.h"
#include "empowerment/empowerment.h"
#include "bomb/bomb.h"
#include <csignal>
#include <functional>

constexpr char* PUT_OLD = ".put_old";
constexpr char* MINISANDBOX_EXEC = ".minisandbox_exec";

namespace minisandbox {

void prepare_procfs() {
    fs::create_directories("/proc");

    if (mount("proc", "/proc", "proc", 0, "")) {
        throw std::runtime_error("Could not mount /proc : " + std::string(strerror(errno)));
    }
}

void killHandler(int signum) {
   std::cerr << "Interrupt signal to sandbox received." << std::endl;
}

void init_child_signal_handlers() {
    signal(SIGINT, killHandler);
    signal(SIGTERM, killHandler);
    signal(SIGSTOP, killHandler);
}

int enter_pivot_root(void* arg) {
    init_child_signal_handlers();
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
    
    if (minisandbox::forkbomb::add_tracer() != 0) {     // TODO: start trace before?
        return 0;                                       // TODO: update it?
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
}

sandbox::~sandbox() {
    try {
        clean_after_run();
    } catch (...) {}
}

std::function<void(int)> kill_child, stop_child, cont_child;
void kill_handler(int signal) { kill_child(signal); }
void stop_handler(int signal) { stop_child(signal); }
void cont_handler(int signal) { cont_child(signal); }

void print_signals_help() {
    std::cout << "To kill app in sandbox send SIGINT or SIGTERM signal to main process, (pid: " << getpid() << ")" << std::endl;
    std::cout << "To stop app in sandbox send SIGTSTP signal to main process, (pid: " << getpid() << ")" << std::endl;
    std::cout << "To continue app in sandbox send SIGCONT signal to main process, (pid: " << getpid() << ")" << std::endl;
}


void init_main_handlers(const pid_t& child) {
    kill_child = [&child](int signum) { 
        kill(-getpgid(child), SIGKILL);
    };

    stop_child = [&child](int signum) { 
        kill(-getpgid(child), SIGSTOP);
    };

    cont_child = [&child](int signum) { 
        kill(-getpgid(child), SIGCONT);
    };

    signal(SIGINT, kill_handler);
    signal(SIGTERM, kill_handler);
    signal(SIGTSTP, stop_handler);
    signal(SIGCONT, cont_handler);
}


void sandbox::run() {
    // TODO: run only elf???
    print_signals_help();
    
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
    void* stack_top = clone_stack + _data.stack_size;

    bind_new_root(_data.rootfs_path.c_str());
    fs::current_path(_data.rootfs_path);

    fs::create_directory(MINISANDBOX_EXEC);

    fs::path exec_in_sandbox = fs::path(MINISANDBOX_EXEC) / _data.executable_path.filename();
    fs::copy_file(_data.executable_path, exec_in_sandbox);
    _data.executable_path = exec_in_sandbox;

    mount_to_new_root(NULL, "/", MS_PRIVATE | MS_REC);

    errno = 0;
    pid_t pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD,
        reinterpret_cast<void*>(&_data)
    );

    init_main_handlers(pid);

    if (pid == -1) {
        throw std::runtime_error(
            "Could not clone process: " +
            std::string(strerror(errno))
        );
    }

    int statloc;
    while (waitpid(pid, &statloc, 0) > 0) {}

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
}

};