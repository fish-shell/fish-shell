function math --description "Perform math calculations in bc"
    set -l scale 0 # default is integer arithmetic

    if set -q argv[1]
        switch $argv[1]
            case '-s*' # user wants to specify the scale of the output
                set scale (string replace -- '-s' '' $argv[1])
                if not string match -q -r '^\d+$' "$scale"
                    echo 'Expected an integer to follow -s' >&2
                    return 2 # missing argument is an error
                end
                set -e argv[1]

            case -h --h --he --hel --help
                __fish_print_help math
                return 0
        end
    end

    if not set -q argv[1]
        return 2 # no arguments is an error
    end

    # With BC_LINE_LENGTH set to 2, the first line is a slash and the following line can be arbitrarily long.
    # We can't set this to 0 because some systems don't have a new enough version of bc.
    set -lx BC_LINE_LENGTH 2
    set -l out (echo "scale=$scale; $argv" | bc)
    set -q out[2]
    and set out $out[2]
    switch "$out"
        case ''
            # No output indicates an error occurred.
            return 3

        case 0
            # For historical reasons a zero result translates to a failure status.
            echo 0
            return 1

        case '*'
            # For historical reasons a non-zero result translates to a success status.
            echo $out
            return 0
    end
end
