function __fish_is_nth_token --description 'Test if current token is the Nth (ignoring command and switches/flags)' --argument-names n
    set -l tokens (commandline -pxc)
    set -l tokens (string replace -r --filter '^([^-].*)' '$1' -- $tokens)
    test (count $tokens) -eq "$n"
end
