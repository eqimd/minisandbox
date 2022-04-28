#include "container/Container.h"

int main() {
    sandbox::Container cont("hello_world", 0, 0, 0);
    cont.run();
}