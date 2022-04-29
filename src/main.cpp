#include "container/Container.h"

int main(int argc, char* argv[]) {
    sandbox::Container cont(argv[1], 0, 0, 0);
    cont.run();
}