# fish completion for spago, PureScript package manager and build tool
# version v0.16.0

spago --fish-completion-script (which spago) | source

function __fish_spago_is_arg_n --argument-names n
    test $n -eq (count (string match -v -- '-*' (commandline -poc)))
end

function __fish_spago_pkgnames
    for line in (spago ls packages 2> /dev/null)
        set pkgname (echo $line | string match -r '[^\s]+')
        echo $pkgname
    end
end

complete -c spago -n '__fish_seen_subcommand_from install; and __fish_spago_is_arg_n 2' -a '(__fish_spago_pkgnames)'
