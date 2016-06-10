function dirh --description "Print the current directory history (the prev and next lists)"
    if set -q argv[1]
        switch $argv[1]
            case -h --h --he --hel --help
                __fish_print_help dirh
                return 0
        end
    end

    set -l dirc (count $dirprev)
    set -l dirprev_rev $dirprev[-1..1]
    for i in (seq $dirc -1 1)
        printf '%2d) %s\n' $i $dirprev_rev[$i]
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
