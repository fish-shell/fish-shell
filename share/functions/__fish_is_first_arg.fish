# determine if this is the very first argument (regardless if switch or not)
function __ghoti_is_first_arg
    set -l tokens (commandline -poc)
    test (count $tokens) -eq 1
end
