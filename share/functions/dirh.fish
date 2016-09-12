function dirh --description "Print the current directory history (the prev and next lists)"
    if set -q argv[1]
        switch $argv[1]
            case -h --h --he --hel --help
                __fish_print_help dirh
                return 0
        end
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
        for i in (seq $dirc)
            printf '%2d) %s\n' $i $dirnext[$i]
        end
    end

    echo
end
