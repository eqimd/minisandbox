#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>
#include <functional>

#include "../../src/bomb/bomb.h"

constexpr int FORK_LIMIT_TESTS = 5;


bool tracee_rec(int forks_remaining, int sleep_time = 1) {
    if (forks_remaining == 0) {
        sleep(sleep_time);
        return true;
    }
    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("fork failed: too many forks");
    }
    if (pid != 0) {
        sleep(sleep_time);
    } else {
        return tracee_rec(forks_remaining - 1, sleep_time);
    }
    return false;
}

bool tracee_cycle(int forks_remaining, int sleep_time = 1) {
    for (int i = 0; i < forks_remaining; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            throw std::runtime_error("fork failed: too many forks");
        }
        if (pid == 0) {
            sleep(sleep_time);
            return false;
        }
    }
    return true;
}


void template_for_test(std::vector<std::function<bool()>> vec_f, int fork_lim = FORK_LIMIT_TESTS, bool failed = false) {
    errno = 0;
    void* _pointer_to = mmap(NULL, sizeof(bool), PROT_WRITE|PROT_READ, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if (_pointer_to == MAP_FAILED)
        throw std::runtime_error("mmap fail: " + std::string(strerror(errno)));
    bool* real_failed = reinterpret_cast<bool*>(_pointer_to);
    *real_failed = false;

    pid_t pid = fork();
    if (pid == -1)
        throw std::runtime_error("fork failed!");
    if (pid == 0) {
        minisandbox::forkbomb::make_tracee();
        for (const auto& f : vec_f) {
            try {
                if (!f())
                    exit(0);
            } catch (const std::runtime_error& e) {
                // std::cout << e.what() << std::endl;
                *real_failed = true;
                exit(0);
            }
        }
        exit(0);
    }
    else {
        minisandbox::forkbomb::tracer(fork_lim);
        assert(failed == *real_failed);
    }

    if(munmap(_pointer_to, sizeof(bool))) {
        throw std::runtime_error("unmmap fail: " + std::string(strerror(errno)));
    }
}


void test_simple_succ() {
    template_for_test({ []{ return tracee_rec(3, 1); } }, 3);
    template_for_test({ []{ return tracee_cycle(3, 1); } }, 3);

    template_for_test({ []{ return tracee_rec(5, 1); } }, 5);
    template_for_test({ []{ return tracee_cycle(5, 1); } }, 5);

    template_for_test({ []{ return tracee_rec(100, 1); } }, 100);
    template_for_test({ []{ return tracee_cycle(100, 1); } }, 100);
}

void test_simple_fail() {
    template_for_test({ []{ return tracee_rec(4, 1); } }, 3, true);
    template_for_test({ []{ return tracee_cycle(4, 1); } }, 3, true);

    template_for_test({ []{ return tracee_rec(6, 1); } }, 5, true);
    template_for_test({ []{ return tracee_cycle(6, 1); } }, 5, true);

    template_for_test({ []{ return tracee_rec(101, 1); } }, 100, true);
    template_for_test({ []{ return tracee_cycle(101, 1); } }, 100, true);
}

void test_scenario_succ() {
    template_for_test({
        []{ return tracee_rec(5, 1); }, 
        []{ return tracee_rec(5, 1); },
        []{ return tracee_rec(5, 1); } 
    }, 5);
    template_for_test({
        []{ return tracee_cycle(5, 1); }, 
        []{ sleep(2); return true; }, 
        []{ return tracee_cycle(5, 1); }, 
        []{ sleep(2); return true; }, 
        []{ return tracee_cycle(5, 1); }, 
    }, 5);
}

void test_scenario_fail() {
    template_for_test({
        []{ return tracee_rec(5, 1); }, 
        []{ return tracee_rec(5, 1); }, 
        []{ return tracee_rec(6, 1); }, 
    }, 5, true);
    template_for_test({
        []{ return tracee_cycle(5, 1); }, 
        []{ sleep(2); return true; }, 
        []{ return tracee_cycle(5, 1); },
        []{ sleep(2); return true; },  
        []{ return tracee_cycle(6, 1); }, 
    }, 5, true);
}


// g++ test_bomb.cpp ../../src/bomb/bomb.cpp -o test_bomb -std=c++17 -lseccomp
int main() {
    test_simple_succ();
    std::cout << "test_simple_succ passed" << std::endl;
    test_simple_fail();
    std::cout << "test_simple_fail passed" << std::endl;
    test_scenario_succ();
    std::cout << "test_scenario_succ passed" << std::endl;
    test_scenario_fail();
    std::cout << "test_scenario_fail passed" << std::endl;

    std::cout << std::endl << "all bomb tests passed" << std::endl;
}
