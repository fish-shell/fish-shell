function __fish_nth_token --description 'Prints the Nth token (ignoring command and switches/flags)' --argument-names n
    set -l tokens (commandline -px | string replace -r --filter '^([^-].*)' '$1')
    # Increment $n by one to account for ignoring the command
    if test (count $tokens) -ge (math "$n" + 1)
        echo $tokens[(math $n + 1)]
    else
        return 1
    end
end
