#include "config.h"  // IWYU pragma: keep

#include "builtin.h"

#include "wutil.h"  // IWYU pragma: keep

/// Counts the number of arguments in the specified null-terminated array
int builtin_count_args(const wchar_t *const *argv) {
    int argc;
    for (argc = 1; argv[argc] != nullptr;) {
        argc++;
    }

    assert(argv[argc] == nullptr);
    return argc;
}

/// This function works like wperror, but it prints its result into the streams.err string instead
/// to stderr. Used by the builtin commands.
void builtin_wperror(const wchar_t *program_name, io_streams_t &streams) {
    char *err = std::strerror(errno);
    if (program_name != nullptr) {
        streams.err()->append(program_name);
        streams.err()->append(L": ");
    }
    if (err != nullptr) {
        const wcstring werr = str2wcstring(err);
        streams.err()->append(werr);
        streams.err()->push(L'\n');
    }
}
