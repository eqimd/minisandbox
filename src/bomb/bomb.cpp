#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <cassert>
#include <vector>
#include <unordered_map>

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::unordered_map;

constexpr int FORK_MAX = 5;
int run = true;


void child2(int i) {
    if (i == 0)
        return;
    cout << "child2 " << i << " start" << endl;
    // if (raise(SIGSTOP))
    //     cerr << "child: raise(SIGSTOP) failed" << endl;
    pid_t pid = fork();
    if (pid < 0)
        cerr << "couldn't fork" << endl;
    else if (pid == 0)
        cout << "child2 " << i << " end" << endl;
    else {
        child2(i-1);
        wait(NULL);
    }
}


void child() {
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0)
        cerr << "child: ptrace(traceme) failed" << endl;
    if (raise(SIGSTOP))
        cerr << "child: raise(SIGSTOP) failed" << endl;

    errno = 0;
    pid_t pid = fork();
    cout << "pid2 " << pid << endl;
    if (pid < 0)
        cerr << "couldn't fork" << endl;
    else if (pid == 0) {
        cout << "in child" << endl;
        // execl("/bin/echo", "/bin/echo", "Hello, world2!", NULL);
    }
    else {
        child2(3);
        wait(NULL);
    }
    // perror("execl");
}


bool add_pid(unordered_map<pid_t, int>& trace_info, pid_t pid) {
    auto [it, was_ins] = trace_info.insert({pid, 0});
    if (!was_ins) {
        cerr << "pid=" << pid << " not inserted" << endl;
        return false;
    }
    if (waitpid(it->first, &it->second, 0) < 0) {
        cerr << "parent: waitpid failed" << endl;
        return false;
    }
    if (!WIFSTOPPED(it->second) || WSTOPSIG(it->second) != SIGSTOP) {
        kill(it->first, SIGKILL);
        cerr << "tracer: unexpected wait status" << endl;
        return false;
    }
    ptrace(PTRACE_SETOPTIONS, it->first, 0, PTRACE_O_TRACEFORK);
    ptrace(PTRACE_SYSCALL, it->first, 0, 0);
    return true;
}

bool add_pid2(unordered_map<pid_t, int>& trace_info, pid_t pid) {
    auto [it, was_ins] = trace_info.insert({pid, 0});
    if (!was_ins) {
        cerr << "pid=" << pid << " not inserted" << endl;
        return false;
    }
    ptrace(PTRACE_ATTACH, it->first, 0, 0);
    if (waitpid(it->first, &it->second, 0) < 0) {
        cerr << "parent: waitpid failed" << endl;
        return false;
    }
    // if (!WIFSTOPPED(it->second) || WSTOPSIG(it->second) != SIGSTOP) {
    //     kill(it->first, SIGKILL);
    //     cerr << "tracer: unexpected wait status" << endl;
    //     return false;
    // }
    ptrace(PTRACE_SETOPTIONS, it->first, 0, PTRACE_O_TRACEFORK);
    ptrace(PTRACE_SYSCALL, it->first, 0, 0);
    return true;
}


void parent(pid_t pid) {
    unordered_map<pid_t, int> trace_info;
    assert(add_pid(trace_info, pid));

    while (run) {
        // cout << trace_info.size() << endl;
        for(auto ti = trace_info.begin(); ti != trace_info.end();){
            if (WIFEXITED(ti->second)) 
                ti = trace_info.erase(ti);
            else 
                ti++;
        }
        if (trace_info.size() > FORK_MAX) {
            for(auto ti = trace_info.begin(); ti != trace_info.end(); ti++){
                kill(ti->first, SIGKILL);
            }
            run = false;
            continue;
        }
        pid_t w_pid;
        for(auto ti = trace_info.begin(); ti != trace_info.end(); ti++){
            if ((w_pid = waitpid(ti->first, &ti->second, WNOHANG)) != -1) {
                assert(w_pid == ti->first || w_pid == 0);
                // if (w_pid != ti->first)
                //     continue;
                if (WIFSTOPPED(ti->second) && (ti->second >> 8) == (SIGTRAP | PTRACE_EVENT_FORK << 8)) {
                    if (((ti->second >> 16) & 0xffff) == PTRACE_EVENT_FORK) {
                        pid_t newpid;
                        if (ptrace(PTRACE_GETEVENTMSG, pid, 0, &newpid) != -1) {        
                            ptrace(PTRACE_CONT, newpid, 0, 0);
                            assert(add_pid2(trace_info, newpid));
                            cout << "newpid " << newpid << endl;    //
                        }
                    }
                }
                ptrace(PTRACE_SYSCALL, ti->first, 0, 0);
            }
        }
    }
}

int main() {
    pid_t pid = fork();
    if (pid != 0)
        cout << "pid " << pid << endl;
    if (pid < 0)
        cerr << "couldn't fork" << endl;
    else if (pid == 0)
        child();
    else
        parent(pid);
    cout << "end" << endl;
    return 0;
}
