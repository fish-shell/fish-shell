# Provide a minimalist realpath implementation to help deal with platforms that may not provide it
# as a command. If an external realpath or grealpath command is available simply pass all arguments
# thru to it. If not fallback to our builtin.

# The following is slightly subtle. We have to define a realpath function even if there is an
# external command by that name. That's because if we don't the parser will select our builtin.
# However, we only want our builtin if there is no external realpath command.

if command -sq realpath
    function realpath -w realpath -d "print the resolved path [command realpath]"
        command realpath $argv
    end
    exit 0
end

# If there is a HomeBrew installed version of GNU realpath named grealpath use that.
if command -sq grealpath
    function realpath -w grealpath -d "print the resolved path [command grealpath]"
        command grealpath $argv
    end
    exit 0
end

function realpath -d "return an absolute path without symlinks"
    if test -z "$argv"
        printf "usage: %s%s%s %sfile%s …\n" (set_color -o) $_ (set_color normal) (set_color -u) (set_color normal)
        echo "       resolves files as absolute paths without symlinks"
        return 1
    end

    # Loop over arguments - allow our realpath to work on more than one path per invocation
    # like gnu/bsd realpath.
    for arg in $argv
        switch $arg
            # These - no can do our realpath
            case -s --strip --no-symlinks -z --zero --relative-base\* --relative-to\*
                __fish_print_help realpath
                return 2

            case -h --help --version
                __fish_print_help realpath
                return 0

                # Handle commands called with these arguments by dropping the arguments to protect
                # realpath from them. There are no sure things here.
            case -e --canonicalize-existing --physical -P -q --quiet -m --canonicalize-missing
                continue

            case "*"
                builtin realpath $arg
        end
    end
end
