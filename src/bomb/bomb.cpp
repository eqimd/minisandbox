#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

#include <unistd.h>
#include <signal.h>
#include <seccomp.h>    // libseccomp-dev

#include <iostream>
#include <thread>
#include <vector>
#include <optional>
#include <atomic>
#include <unordered_map>
#include <string.h>
#include <signal.h>
#include <string>

#include "bomb.h"


namespace minisandbox::forkbomb {

struct pid_status {
    enum state {
        k_after_sys_call,
        k_before_sys_call  
    };
    
    pid_t pid;
    state current_st;
    int lastsysno = 0;

    pid_status(pid_t pid_) : pid { pid_ }, current_st { k_after_sys_call } {
        ptrace(PTRACE_SETOPTIONS, 
               pid, 
               0, 
                PTRACE_O_TRACESYSGOOD 
               | PTRACE_O_TRACESECCOMP 
               | PTRACE_O_TRACEFORK 
               | PTRACE_O_TRACECLONE);
    }

    std::optional<user_regs_struct> handle_status(int status) {
        if (current_st == k_after_sys_call && (status >> 8) == (SIGTRAP | (PTRACE_EVENT_SECCOMP << 8))) {
            user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);
            current_st = k_before_sys_call;
            lastsysno = regs.orig_rax;
            return regs;
        } else if (WIFSTOPPED(status) && WSTOPSIG(status) & (1u << 7)) {
            user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);
            current_st = k_after_sys_call;
            return regs;
        } else if (WIFSTOPPED(status)) {
            if (WSTOPSIG(status) == SIGSEGV) {
                throw std::runtime_error("SIGSEGV was thrown in child process");
            }
        } else if (WIFSIGNALED(status)) {
            if (WTERMSIG(status) == SIGKILL) {
                throw std::runtime_error("Process was killed");
            }
        }

        return std::nullopt;
    }

    void resume() {
        if (current_st == k_before_sys_call) {
           // await syscall exit
           ptrace(PTRACE_SYSCALL, pid, 0, 0);
        } else {
           // await next signal/seccomp event 
           ptrace(PTRACE_CONT, pid, 0, 0);
        }
    }

    void set_registers(const user_regs_struct& st) {
        ptrace(PTRACE_SETREGS, pid, 0, &st);
    }
};


void tracer(int fork_limit) {
    int fork_limit_left = fork_limit;

    std::unordered_map<pid_t, pid_status> tracees;

    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid < 0) {
            break;
        }
        if (WIFEXITED(status)) {
            tracees.erase(pid);
            fork_limit_left++;
            continue;
        }

        if (tracees.count(pid) == 0) {
            tracees.emplace(pid, pid_status { pid });
            tracees.at(pid).resume();
            continue;
        }

        {
            auto& pid_state = tracees.at(pid);
            auto regs = pid_state.handle_status(status);
            if (regs && pid_state.current_st == pid_status::k_before_sys_call) {
                auto st = *regs;
                if (pid_state.lastsysno == SYS_clone || pid_state.lastsysno == SYS_fork) {
                    if (fork_limit_left <= 0) {
                        std::cerr << "Executable tried to fork " << fork_limit + 1 << " times, but it is allowed to fork only " << fork_limit << " times" << std::endl;
                        st.orig_rax = -1;
                        pid_state.set_registers(st);
                    }
                    fork_limit_left--;
                }
            } else if (regs && pid_state.current_st == pid_status::k_after_sys_call) {
                auto st = *regs;
                if (st.orig_rax == -1) {
                    st.rax = -EPERM;
                    pid_state.set_registers(st);
                }
            }
        }
        tracees.at(pid).resume();
    }
}

void set_filters() {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    seccomp_rule_add(ctx, SCMP_ACT_TRACE(0), SYS_clone, 0);
    seccomp_rule_add(ctx, SCMP_ACT_TRACE(0), SYS_fork, 0);
    seccomp_load(ctx);
}

void make_tracee() {
    errno = 0;
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
        throw std::runtime_error(
            "Call to make_tracee() failed: ptrace(traceme) failed, " +
            std::string(strerror(errno))
        );
    }

    set_filters();
    
    /* Остановиться и дождаться, пока отладчик отреагирует. */
    if (raise(SIGSTOP)) {
        std::runtime_error("main: raise(SIGSTOP) failed");
    }
}

};
