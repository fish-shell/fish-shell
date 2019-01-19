#include <wchar.h>
#include <iostream>

int main(int argc, char** argv) {
    // This initializes the locale according to the environment variables.
    // That means if the environment isn't set up for unicode, this whole excercise is pointless.
    setlocale(LC_ALL, "");
    std::cout << wcwidth(L'😃');
    return 0;
}
