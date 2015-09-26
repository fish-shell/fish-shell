function __fish_complete_pushd_plus
        if count $dirstack > /dev/null
                # TODO: Shift to using string replace when released
                #
                # print each member of the stack, replace $HOME with ~
                for i in (seq (count $dirstack))
                        echo +$i\tRotate to (echo $dirstack[$i] | sed -e "s|^$HOME|~|")
                end
        end
end

function __fish_complete_pushd_swap
        if count $dirstack > /dev/null
                # TODO: Shift to using string replace when released
                #
                # replace $HOME with ~
                echo ''\tSwap with (echo $dirstack[1] | sed -e "s|^$HOME|~|")
        end
end

# support pushd <dir>
complete -c pushd -a "(__fish_complete_cd)"

# support pushd <>
complete -c pushd -a '(__fish_complete_pushd_swap)'

# support pushd <+n>
complete -c pushd -a '(__fish_complete_pushd_plus)'
