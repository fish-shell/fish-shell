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

    # Set BC_LINE_LENGTH to a ridiculously high number so it only uses one line for most results.
    # We can't use 0 since some systems (including macOS) use an ancient bc that doesn't support it.
    # We also can't count on this being recognized since some BSD systems don't recognize this env
    # var at all and limit the line length to 70.
    set -lx BC_LINE_LENGTH 500
    set -l out (echo "scale=$scale; $argv" | bc)
    if set -q out[2]
        set out (string join '' (string replace \\ '' $out))
    end
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
