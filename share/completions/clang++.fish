# Multiple versions are often installed as clang++, clang++-7, clang++-8, etc.
# They won't be autoloaded, but once clang++ is used once, they'll gain completions too.
# This could potentially be moved to __ghoti_config_interactive.ghoti in the future.

complete -p '*clang++*' -n __ghoti_should_complete_switches -xa '(__ghoti_complete_clang)'
complete -p '*clang++*' -n 'not __ghoti_should_complete_switches' \
    -k -xa "(__ghoti_complete_suffix .o .out .c .cpp .so .dylib)"
