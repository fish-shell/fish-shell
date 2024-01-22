# fish completion for spago, PureScript package manager and build tool
# version v0.16.0

spago --fish-completion-script (command -v spago) | source

function __fish_spago_is_arg_n --argument-names n
    test $n -eq (count (string match -v -- '-*' (commandline -pxc)))
end

function __fish_spago_pkgnames
    spago ls packages 2>/dev/null | string match -r '\S+'
end

complete -c spago -n '__fish_seen_subcommand_from install; and __fish_spago_is_arg_n 2' -a '(__fish_spago_pkgnames)'
