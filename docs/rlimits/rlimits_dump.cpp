#include <iostream>
#include <sys/resource.h>
#include <map>
#include <cstring>

rlimit get_rlimit(int pid, __rlimit_resource res) {
    rlimit rlim;
    int ret_code = prlimit(pid, res, NULL, &rlim);
    if (ret_code != 0) {
        throw std::runtime_error("prlimit error: " + std::string(strerror(errno)));
    }

    return rlim;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "No PID argument found" << std::endl;
        return EXIT_FAILURE;
    }
    int pid = std::stoi(argv[1]);
    

    std::map<__rlimit_resource, std::string> rs = {
        {RLIMIT_AS, "RLIMIT_AS: maximum size of the process's virtual memory"},
        {RLIMIT_CORE, "RLIMIT_CORE: maximum size of a core file in bytes that the process may dump"},
        {RLIMIT_CPU, "RLIMIT_CPU: limit, in seconds, on the amount of CPU time that the process can consume"},
        {RLIMIT_DATA, "RLIMIT_DATA: maximum size of the process's data segment (initialized data, uninitialized data, and heap)."},
        {RLIMIT_FSIZE, "RLIMIT_FSIZE: maximum size in bytes of files that the process may create"},
        {RLIMIT_LOCKS, "RLIMIT_LOCKS: limit on the combined number of `flock` locks and `fcntl` leases that this process may establish"},
        {RLIMIT_MEMLOCK, "RLIMIT_MEMLOCK: maximum number of bytes of memory that may be locked into RAM"},
        {RLIMIT_MSGQUEUE, "RLIMIT_MSGQUEUE: limit on the number of bytes that can be allocated for POSIX message queues for the real user ID of the calling process"},
        {RLIMIT_NICE, "RLIMIT_NICE: ceiling to which the process's nice value can be raised using `setpriority` or `nice`"},
        {RLIMIT_NOFILE, "RLIMIT_NOFILE: value one greater than the maximum file descriptor number that can be opened by this process"},
        {RLIMIT_NPROC, "RLIMIT_NPROC: limit on the number of extant process (or, more precisely on Linux, threads) for the real user ID of the calling process"},
        {RLIMIT_RSS, "RLIMIT_RSS: limit (in bytes) on the process's resident set (the number of virtual pages resident in RAM)"},
        {RLIMIT_RTPRIO, "RLIMIT_RTPRIO: ceiling on the real-time priority that may be set for this process using `sched_setscheduler` and `sched_setparam`"},
        {RLIMIT_RTTIME, "RLIMIT_RTTIME: limit (in microseconds) on the amount of CPU time that a process scheduled under a real-time scheduling policy may consume without making a blocking system call"},
        {RLIMIT_SIGPENDING, "RLIMIT_SIGPENDING: limit on the number of signals that may be queued for the real user ID of the calling process"},
        {RLIMIT_STACK, "RLIMIT_STACK: maximum size of the process stack, in bytes"},
    };

    try {
        for (const auto &res: rs) {
            errno = 0;
            
            rlimit rlim = get_rlimit(pid, res.first);

            std::cout << res.second << std::endl;
            std::cout << "\tSoft: " << rlim.rlim_cur << std::endl;
            std::cout << "\tHard: " << rlim.rlim_max << std::endl;
        }
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}