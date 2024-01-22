# determine if this is the very first argument (regardless if switch or not)
function __fish_is_first_arg
    set -l tokens (commandline -pxc)
    test (count $tokens) -eq 1
end
