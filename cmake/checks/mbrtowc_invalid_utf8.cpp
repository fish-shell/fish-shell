#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

// Check whether the runtime mbrtowc implementation attempts to encode
// invalid UTF-8 values.

int main() {
    // TODO: I'm not sure how to enforce a UTF-8 locale without overriding the language
    char sample[] = "hello world";
    sample[0] |= 0xF8;
    wchar_t wsample[100] {};
    std::mbstate_t state = std::mbstate_t();
    int res = std::mbrtowc(wsample, sample, strlen(sample), &state);

    return res < 0 ? 0 : 1;
}
