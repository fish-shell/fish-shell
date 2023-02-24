# Multiple versions are often installed as clang++, clang++-7, clang++-8, etc.
# They won't be autoloaded, but once clang++ is used once, they'll gain completions too.
# This could potentially be moved to __fish_config_interactive.fish in the future.

complete -p '*clang++*' -n __fish_should_complete_switches -xa '(__fish_complete_clang)'
complete -p '*clang++*' -n 'not __fish_should_complete_switches' \
    -k -xa "(__fish_complete_suffix .o .out .c .cpp .so .dylib)"
