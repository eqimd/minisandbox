# Rlimits
## API

Three main functions to manage rlimits:
```c
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

int prlimit(pid_t pid, int resource, const struct rlimit *new_limit,
            struct rlimit *old_limit);
```

`getrlimit` and `setrlimit` allow to manage resource limits of the current process, while `prlimit` gives an opportunity to access `rlimits` of process with `pid` pid.

`resource` stands for resource type. There are several resource types that could be managed.

`rlimit` structure has the following fields:
```c
struct rlimit {
    rlim_t rlim_cur;  /* Soft limit */
    rlim_t rlim_max;  /* Hard limit (ceiling for rlim_cur) */
};
```

`rlim_cur` or `Soft limit` is the value that the kernel enforces for the corresponding resource, so we are not allowed to exceed this value.

`rlim_max` or `Hard limit` is the max value for the `Soft limit`. Can be changed only by a privileged process.

## Rlimits types
`rlimits_dump.cpp` -- program to dump rlimits of the process with `PID` pid.
### Example
```shell
$ ./a.out 86539
RLIMIT_CPU: limit, in seconds, on the amount of CPU time that the process can consume
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_FSIZE: maximum size in bytes of files that the process may create
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_DATA: maximum size of the process's data segment (initialized data, uninitialized data, and heap).
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_STACK: maximum size of the process stack, in bytes
        Soft: 8388608
        Hard: 18446744073709551615
RLIMIT_CORE: maximum size of a core file in bytes that the process may dump
        Soft: 0
        Hard: 18446744073709551615
RLIMIT_RSS: limit (in bytes) on the process's resident set (the number of virtual pages resident in RAM)
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_NPROC: limit on the number of extant process (or, more precisely on Linux, threads) for the real user ID of the calling process
        Soft: 127893
        Hard: 127893
RLIMIT_NOFILE: value one greater than the maximum file descriptor number that can be opened by this process
        Soft: 1048576
        Hard: 1048576
RLIMIT_MEMLOCK: maximum number of bytes of memory that may be locked into RAM
        Soft: 67108864
        Hard: 67108864
RLIMIT_AS: maximum size of the process's virtual memory
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_LOCKS: limit on the combined number of `flock` locks and `fcntl` leases that this process may establish
        Soft: 18446744073709551615
        Hard: 18446744073709551615
RLIMIT_SIGPENDING: limit on the number of signals that may be queued for the real user ID of the calling process
        Soft: 127893
        Hard: 127893
RLIMIT_MSGQUEUE: limit on the number of bytes that can be allocated for POSIX message queues for the real user ID of the calling process
        Soft: 819200
        Hard: 819200
RLIMIT_NICE: ceiling to which the process's nice value can be raised using `setpriority` or `nice`
        Soft: 0
        Hard: 0
RLIMIT_RTPRIO: ceiling on the real-time priority that may be set for this process using `sched_setscheduler` and `sched_setparam`
        Soft: 0
        Hard: 0
RLIMIT_RTTIME: limit (in microseconds) on the amount of CPU time that a process scheduled under a real-time scheduling policy may consume without making a blocking system call
        Soft: 18446744073709551615
        Hard: 18446744073709551615
```

Here we can inspect all the resources types. For our needs, the most interest types are `RLIMIT_CPU`, `RLIMIT_DATA`, `RLIMIT_NPROC` and `RLIMIT_MEMLOCK`(?).

## Sources
https://man7.org/linux/man-pages/man2/getrlimit.2.html
