function dirh --description "Print the current directory history (the prev and next lists)"
    set -l options h/help
    argparse -n dirh --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help dirh
        return 0
    end

    set -l dirc (count $dirprev)
    if test $dirc -gt 0
        set -l dirprev_rev $dirprev[-1..1]
        # This can't be (seq $dirc -1 1) because of BSD.
        set -l dirnum (seq 1 $dirc)
        for i in $dirnum[-1..1]
            printf '%2d) %s\n' $i $dirprev_rev[$i]
        end
    end

    echo (set_color $fish_color_history_current)'   ' $PWD(set_color normal)

    set -l dirc (count $dirnext)
    if test $dirc -gt 0
        set -l dirnext_rev $dirnext[-1..1]
        for i in (seq $dirc)
            printf '%2d) %s\n' $i $dirnext_rev[$i]
        end
    end
    echo
end
