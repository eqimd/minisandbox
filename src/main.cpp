#include "sandbox/Sandbox.h"

int main(int argc, char* argv[]) {
    sandbox_ns::Sandbox cont(argv[1], 0, 0, 0);
    cont.run();
}