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
    set -l options 'h/help' 'q/quiet' 'V-version' 's/strip' 'N-no-symlinks' 'z/zero'
    set options $options 'e/canonicalize-existing' 'm/canonicalize-missing' 'L/logical' 'P/physical'
    set options $options 'R-relative-to=' 'B-relative-base='
    argparse -n realpath --min-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help realpath
        return 0
    end

    # We don't implement any of the other flags so if any are set it's an error.
    if string match -q '_flag_*' -- (set -l)
        set flags (set -l | string replace --filter _flag_ '')
        printf (_ "%s: These flags are not allowed by fish realpath: '%s'") realpath "$flags" >&2
        echo >&2
        __fish_print_help realpath
        return 1
    end

    for path in $argv
        builtin realpath $path
    end
end
