function popd --description "Pop directory from the stack and cd to it"
    set -l options 'h/help'
    argparse -n popd --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help popd
        return 0
    end

    if set -q dirstack[1]
        cd $dirstack[1]
        set -e dirstack[1]
    else
        printf (_ "%s: Directory stack is empty…\n") popd 1>&2
        return 1
    end
end
