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


constexpr char* PUT_OLD = ".put_old";
constexpr char* MINISANDBOX_EXEC = ".minisandbox_exec";

namespace minisandbox {

struct clone_data {
    const char* executable;
    char** argv;
    char** envp;
};

void drop_privileges() {
    gid_t gid;
	uid_t uid;

	// no need to "drop" the privileges that you don't have in the first place!
	if (getuid() != 0) {
		return;
	}

	// when your program is invoked with sudo, getuid() will return 0 and you
	// won't be able to drop your privileges
	if ((uid = getuid()) == 0) {
		const char *sudo_uid = secure_getenv("SUDO_UID");
		if (sudo_uid == NULL) {
			printf("environment variable `SUDO_UID` not found\n");
			return;
		}
		errno = 0;
		uid = (uid_t) strtoll(sudo_uid, NULL, 10);
		if (errno != 0) {
			perror("under-/over-flow in converting `SUDO_UID` to integer");
			return;
		}
	}

	// again, in case your program is invoked using sudo
	if ((gid = getgid()) == 0) {
		const char *sudo_gid = secure_getenv("SUDO_GID");
		if (sudo_gid == NULL) {
			printf("environment variable `SUDO_GID` not found\n");
			return;
		}
		errno = 0;
		gid = (gid_t) strtoll(sudo_gid, NULL, 10);
		if (errno != 0) {
			perror("under-/over-flow in converting `SUDO_GID` to integer");
			return;
		}
	}

	if (setgid(gid) != 0) {
		perror("setgid");
		return;
	}
	if (setuid(uid) != 0) {
		perror("setgid");
		return;
	}

	// change your directory to somewhere else, just in case if you are in a
	// root-owned one (e.g. /root)
	if (chdir("/") != 0) {
		perror("chdir");
		return;
	}

	// check if we successfully dropped the root privileges
	if (setuid(0) == 0 || seteuid(0) == 0) {
		printf("could not drop root privileges!\n");
		return;
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

    errno = 0;
    if (umount2(PUT_OLD, MNT_DETACH) == -1) {
        throw std::runtime_error(
            "Could not unmount old root: " +
            std::string(strerror(errno))
        );
    }
    fs::remove(PUT_OLD);

    drop_privileges();

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
    time_execution_limit_ms = sb_data.time_execution_limit_ms;
    ram_limit_bytes = sb_data.ram_limit_bytes;
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
    void* stack_top = clone_stack + stack_size;

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

    fs::create_directory("programming");
    mount_to_new_root("/home/eqimd/programming", "programming", MS_BIND);

    struct clone_data data = {};
    data.executable = exec_in_sandbox.c_str();
    data.argv = const_cast<char**>(argv_cstr.data());
    data.envp = const_cast<char**>(envp_cstr.data());

    errno = 0;
    pid_t pid = clone(
        enter_pivot_root,
        stack_top,
        CLONE_NEWNS | CLONE_NEWPID | SIGCHLD,
        &data
    );
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
        munmap(clone_stack, stack_size);
        clone_stack = nullptr;
    }
}

};