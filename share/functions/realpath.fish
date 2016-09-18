# Provide a minimalist realpath implementation to help deal with platforms that may not provide it
# as a command. If a realpath command is available simply pass all arguments thru to it. If not
# fallback to alternative solutions.

# The following is slightly subtle. The first time `realpath` is invoked this script will be read.
# If we see that there is an external command by that name we just return. That will cause fish to
# run the external command. On the other hand, if an external command isn't found we define a
# function that will provide fallback behavior.
if not command -s realpath >/dev/null

    if command -s grealpath >/dev/null
        function realpath -w grealpath -d "print the resolved path [grealpath]"
            grealpath $argv
        end
    else
        function realpath -w fish_realpath -d "get an absolute path without symlinks [fish_realpath]"
            if test -z $argv
                printf "usage: %s%s%s %sfile%s …\n" (set_color -o) $_ (set_color normal) (set_color -u) (set_color normal)
                echo "       resolves files as absolute paths without symlinks"
                return 1
            end

            for arg in $argv
                switch $arg
                    # These - no can do our realpath
                    case -s --strip --no-symlinks -z --zero --relative-base\* --relative-to\*
                        __fish_print_help fish_realpath
                        return 2

                    case -h --help --version
                        __fish_print_help fish_realpath
                        return 0

                        # Service commands called with these arguments by
                        # dropping the arguments to protext fish_realpath from them
                        # There are no sure things here
                    case -e --canonicalize-existing --physical -P -q --quiet -m --canonicalize-missing
                        continue

                    case "*"
                        fish_realpath $argv
                end
            end
        end
    end
end
