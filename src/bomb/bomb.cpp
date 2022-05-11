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


#define die(...)                                           \
    do                                                     \
    {                                                      \
        fprintf(stderr, "[E:%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                      \
        fprintf(stderr, "\n");                             \
        exit(1);                                           \
    } while (0)


constexpr int FORK_LIMIT_DEFAULT = 5;


struct PidStatus {
    enum State {
        kAfterSysCall,
        kBeforeSysCall  
    };
    
    pid_t pid;
    State current_state;
    int lastsysno = 0;

    PidStatus(pid_t pid_) : pid { pid_ }, current_state { kAfterSysCall } {
        // std::cout << "TRACE NEW PID=" << pid << std::endl;
        ptrace(PTRACE_SETOPTIONS, 
               pid, 
               0, 
                PTRACE_O_TRACESYSGOOD 
               | PTRACE_O_TRACESECCOMP 
               | PTRACE_O_TRACEFORK 
               | PTRACE_O_TRACECLONE);
    }

    std::optional<user_regs_struct> HandleStatus(int status) {
        if (current_state == kAfterSysCall) {
            if ((status >> 8) == (SIGTRAP | (PTRACE_EVENT_SECCOMP << 8))) {
                user_regs_struct regs;
                ptrace(PTRACE_GETREGS, pid, 0, &regs);
                current_state = kBeforeSysCall;
                lastsysno = regs.orig_rax;
                return regs;
            }
        } else {
            if (WIFSTOPPED(status) && WSTOPSIG(status) & (1u << 7)) {
                user_regs_struct regs;
                ptrace(PTRACE_GETREGS, pid, 0, &regs);
                current_state = kAfterSysCall;
                return regs;
            }
        }
        return std::nullopt;
    }

    void Resume() {
        if (current_state == kBeforeSysCall) {
           // await syscall exit
           ptrace(PTRACE_SYSCALL, pid, 0, 0);
        } else {
           // await next signal/seccomp event 
           ptrace(PTRACE_CONT, pid, 0, 0);
        }
    }

    void SetRegisters(const user_regs_struct& state) {
        ptrace(PTRACE_SETREGS, pid, 0, &state);
    }
};


void tracer(int fork_limit = FORK_LIMIT_DEFAULT) {
    if (fork_limit < 0) die("wrong fork limit count");
    int fork_limit_left = fork_limit;

    std::unordered_map<pid_t, PidStatus> tracees;
   
    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid < 0) {
            break;
        }
        if (WIFEXITED(status)) {
            tracees.erase(pid);
            continue;
        }

        if (tracees.count(pid) == 0) {
            tracees.emplace(pid, PidStatus { pid });
            tracees.at(pid).Resume();
            continue;
        }

        {
            auto& pid_state = tracees.at(pid);
            auto regs = pid_state.HandleStatus(status);
            if (regs && pid_state.current_state == PidStatus::kBeforeSysCall) {
                auto state = *regs;
                if (pid_state.lastsysno == SYS_clone || pid_state.lastsysno == SYS_fork) {
                    if (fork_limit_left == 0) {
                        state.orig_rax = -1;
                        pid_state.SetRegisters(state);
                    } else {
                        fork_limit_left--;
                    }
                }
            } else if (regs && pid_state.current_state == PidStatus::kAfterSysCall) {
                auto state = *regs;
                if (state.orig_rax == -1) {
                    state.rax = -EPERM;
                    pid_state.SetRegisters(state);
                }
            }
        }
        tracees.at(pid).Resume();
    }
}

// for visualisation only, remove later
void tracee(int forks_remaining) {
    // std::cout << "tracee: " << forks_remaining << std::endl;
    if (forks_remaining == 0) {
        sleep(10);
        return;
    }
    // std::cout << "tracee: " << forks_remaining << " fork!" << std::endl;
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "fork failed: too many forks" << std::endl;
        return;
    }
    if (pid != 0) {
        sleep(10);
    } else {
        tracee(forks_remaining - 1);
    }
}


int add_tracer() {
    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "add_tracer failed" << std::endl;
    }
    if (pid != 0) {
        tracer();
    } else {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
            die("main: ptrace(traceme) failed: %m");
        }

        scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
        seccomp_rule_add(ctx, SCMP_ACT_TRACE(0), SYS_clone, 0);
        seccomp_rule_add(ctx, SCMP_ACT_TRACE(0), SYS_fork, 0);
        seccomp_load(ctx);
        
        /* Остановиться и дождаться, пока отладчик отреагирует. */
        if (raise(SIGSTOP)) {
            die("main: raise(SIGSTOP) failed: %m");
        }
    }
    return pid;
}


int main() {                    // add fork_limit option
    if (add_tracer() != 0) {
        return 0;               // update it?
    }

    // other code
    tracee(7);                  // remove it later
    return 0;
}
