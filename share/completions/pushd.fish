function __fish_complete_pushd_plus
    if count $dirstack >/dev/null
        # print each member of the stack, replace $HOME with ~
        for i in (seq (count $dirstack))
            printf "+%s\t%s\n" $i "Rotate to "(string replace -r "^$HOME" "~" -- $dirstack[$i])
        end
    end
end

function __fish_complete_pushd_minus
    if count $dirstack >/dev/null
        # print each member of the stack, replace $HOME with ~
        # Negative arguments are expected to start at "-0"
        for i in (seq (count $dirstack) -1 1)
            printf "%s\t%s\n" -(math $i - 1) "Rotate to "(string replace -r "^$HOME" "~" -- $dirstack[(math -$i)])
        end
    end
end

function __fish_complete_pushd_swap
    if count $dirstack >/dev/null
        # replace $HOME with ~
        printf "\t%s\n" "Swap with "(string replace -r "^$HOME" "~" -- $dirstack[1])
    end
end

# support pushd <dir>
complete -c pushd -a "(__fish_complete_cd)"

# support pushd <>
complete -c pushd -a '(__fish_complete_pushd_swap)'

# support pushd <+n>
complete -c pushd -a '(__fish_complete_pushd_plus)'
complete -c pushd -a '(__fish_complete_pushd_minus)'
