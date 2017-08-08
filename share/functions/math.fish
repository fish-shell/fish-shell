function math --description "Perform math calculations in bc"
    if not set -q argv[2]
        # Make sure an invocation like `math "-1 + 1"` doesn't treat the string as an option.
        set argv -- $argv
    end

    set -l options 'h/help' 's/scale=!_validate_int --min=0' '#-val'
    argparse -n math --stop-nonopt --min-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help math
        return 0
    end

    set -l scale 0 # default is integer arithmetic
    set -q _flag_scale
    and set scale $_flag_scale

    if set -q _flag_val
        # The expression began with a negative number. Put it back in the expression.
        # The correct thing is for the person calling us to insert a `--` separator before the
        # expression to stop parsing flags. But we'll work around that missing token here.
        set argv -$_flag_val $argv
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
