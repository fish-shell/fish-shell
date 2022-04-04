function prevd --description "Move back in the directory history"
    set -l options h/help l/list
    argparse -n prevd --max-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help prevd
        return 0
    end

    set -l times 1
    if set -q argv[1]
        if test $argv[1] -ge 0 2>/dev/null
            set times $argv[1]
        else
            printf (_ "%s: The number of positions to skip must be a non-negative integer\n") prevd >&2
            return 1
        end
    end

    # Traverse history
    set -l code 1
    for i in (seq $times)
        # Try one step forward
        if __fish_move_last dirprev dirnext
            # We consider it a success if we were able to do at least 1 step
            # (low expectations are the key to happiness ;)
            set code 0
        else
            break
        end
    end

    # Show history if needed
    if set -q _flag_list
        dirh
    end

    # Set direction for 'cd -'
    test $code = 0
    and set -g __fish_cd_direction next

    return $code
end
