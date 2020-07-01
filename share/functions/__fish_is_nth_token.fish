function __fish_is_nth_token
    set -l n $argv[1]
    set -l tokens (commandline -poc)
    set -l tokens (string replace -r --filter '^([^-].*)' '$1' -- $tokens)
    test (count $tokens) -eq "$n"
end
