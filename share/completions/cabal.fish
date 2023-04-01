function __ghoti_complete_cabal
    set -l cmd (commandline -poc)
    if test (count $cmd) -gt 1
        cabal $cmd[2..-1] --list-options
    else
        cabal --list-options
    end
end

complete -c cabal -a '(__ghoti_complete_cabal)'
