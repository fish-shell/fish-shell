# returns 0 only if previous argument is one of the supplied arguments
function __fish_prev_arg_in
    set -l tokens (commandline -cx)
    set -l tokenCount (count $tokens)
    if test $tokenCount -lt 2
        # need at least cmd and prev argument
        return 1
    end
    for arg in $argv
        if string match -q -- $tokens[-1] $arg
            return 0
        end
    end

    return 1
end
