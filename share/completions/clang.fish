# Multiple versions are often installed as clang, clang-7, clang-8, etc.
# They won't be autoloaded, but once clang++ is used once, they'll gain completions too.
# This could potentially be moved to __fish_config_interactive.fish in the future.

# This pattern unfortunately matches clang-format, etc. as well.
complete -p '*clang*' -n '__fish_should_complete_switches' -xa '(__fish_clang_complete)'
complete -c 'clang' -n 'not __fish_should_complete_switches' \
    -xa "(__fish_complete_suffix .o; __fish_complete_suffix .out; __fish_complete_suffix .c; __fish_complete_suffix .cpp; __fish_complete_suffix .so; __fish_complete_suffix .dylib)"

# again but without the -x this time for the pattern-matched completion
complete -p '*clang*' -n 'not __fish_should_complete_switches' \
    -a "(__fish_complete_suffix .o; __fish_complete_suffix .out; __fish_complete_suffix .c; __fish_complete_suffix .cpp; __fish_complete_suffix .so; __fish_complete_suffix .dylib)"
