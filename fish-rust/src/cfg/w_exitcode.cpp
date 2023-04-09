#include <sys/wait.h>

int main() {
    static_assert(WEXITSTATUS(0x007f) == 0x7f, "");
    return 0;
}
