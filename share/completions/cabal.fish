function __fish_complete_cabal
        if type -q -f cabal
                set cmd (commandline -poc)
                if test (count $cmd) -gt 1
                        cabal $cmd[2..-1] --list-options
                else
                        cabal --list-options
                end
        end
end

complete -c cabal -a '(__fish_complete_cabal)'
