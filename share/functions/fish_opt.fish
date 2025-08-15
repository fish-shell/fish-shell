# This is a helper function for `fish_opt`. It does some basic validation of the arguments.
function __fish_opt_validate_args --no-scope-shadowing
    if not set -q _flag_validate && test (count $argv) -ne 0
        printf (_ "%s: Extra non-option arguments were provided\n") fish_opt >&2
        return 1
    else if set -q _flag_validate && test (count $argv) -eq 0
        printf (_ "%s: The --validate flag requires subsequent arguments\n") fish_opt >&2
        return 1
    else if set -q _flag_validate && not set -q _flag_multiple_vals && not set -q _flag_optional_val && not set -q _flag_required_val
        printf (_ "%s: The --validate flag requires the --required-val, --optional-value, or --multiple-vals flag\n") fish_opt >&2
        return 1
    else if set -q _flag_short && test 1 -ne (string length -- $_flag_short)
        printf (_ "%s: The --short flag must be a single character\n") fish_opt >&2
        return 1
    else if not set -q _flag_short && not set -q _flag_long
        set -S _flag_short >&2
        printf (_ "%s: Either the --short or --long flag must be provided\n") fish_opt >&2
        return 1
    else if set -q _flag_long_only && not set -q _flag_long
        printf (_ "%s: The --long-only flag requires the --long flag\n") fish_opt >&2
        return 1
    end

    return 0
end

# The `fish_opt` command.
function fish_opt -d 'Produce an option specification suitable for use with `argparse`.'
    set -l options h/help 's/short=' 'l/long=' d/delete o/optional-val r/required-val m/multiple-vals long-only v/validate
    argparse -n fish_opt --stop-nonopt --exclusive=r,o $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help fish_opt
        return 0
    end

    __fish_opt_validate_args $argv
    or return

    if not set -q _flag_short
        set opt_spec "/$_flag_long"
    else if not set -q _flag_long
        set opt_spec $_flag_short
    else if set -q _flag_long_only
        set opt_spec "$_flag_short-$_flag_long"
    else
        set opt_spec "$_flag_short/$_flag_long"
    end

    if set -q _flag_multiple_vals && set -q _flag_optional_val
        set opt_spec "$opt_spec=*"
    else if set -q _flag_multiple_vals
        set opt_spec "$opt_spec=+"
    else if set -q _flag_required_val
        set opt_spec "$opt_spec="
    else if set -q _flag_optional_val
        and set opt_spec "$opt_spec=?"
    end

    if set -q _flag_delete
        set opt_spec "$opt_spec&"
    end

    if set -q _flag_validate
        set opt_spec "$opt_spec!$argv"
    end

    echo $opt_spec
end
