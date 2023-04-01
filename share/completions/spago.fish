# ghoti completion for spago, PureScript package manager and build tool
# version v0.16.0

spago --ghoti-completion-script (command -v spago) | source

function __ghoti_spago_is_arg_n --argument-names n
    test $n -eq (count (string match -v -- '-*' (commandline -poc)))
end

function __ghoti_spago_pkgnames
    spago ls packages 2>/dev/null | string match -r '\S+'
end

complete -c spago -n '__ghoti_seen_subcommand_from install; and __ghoti_spago_is_arg_n 2' -a '(__ghoti_spago_pkgnames)'
