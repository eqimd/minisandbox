#pragma once

constexpr int FORK_LIMIT_DEFAULT = 5;

namespace minisandbox::forkbomb {

void make_tracee();
void tracer(int fork_limit);

};
