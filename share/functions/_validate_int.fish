# This function is intended to be used as a validation command for individual option specifications
# given to the `argparse` command. It checks that the argument is a valid integer and optionally
# whether it is in a reasonable range.
function _validate_int --no-scope-shadowing
    # Note that we can't use ourself to validate the arguments to the --min and --max flags this
    # function recognizes.
    set -l options 'm-min=' 'x-max='
    argparse -n _argparse_validate_int $options -- $argv
    or return

    if not string match -qr '^-?\d+$' -- $_flag_value
        set -l msg (_ "%s: Value '%s' for flag '%s' is not an integer\n")
        printf $msg $_argparse_cmd $_flag_value $_flag_name >&2
        return 1
    end

    if set -q _flag_min
        and test $_flag_value -lt $_flag_min
        set -l msg (_ "%s: Value '%s' for flag '%s' less than min allowed of '%s'\n")
        printf $msg $_argparse_cmd $_flag_value $_flag_name $_flag_min >&2
        return 1
    end

    if set -q _flag_max
        and test $_flag_value -gt $_flag_max
        set -l msg (_ "%s: Value '%s' for flag '%s' greater than max allowed of '%s'\n")
        printf $msg $_argparse_cmd $_flag_value $_flag_name $_flag_max >&2
        return 1
    end

    return 0
end
