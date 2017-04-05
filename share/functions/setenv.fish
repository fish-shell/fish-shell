function setenv --description 'Set an env var for csh compatibility.'
    # No arguments should cause the current env vars to be displayed.
    if not set -q argv[1]
        env
        return
    end

    # A single argument should set the named var to nothing.
    if not set -q argv[2]
        set -gx $argv[1] ''
        return
    end

    # `setenv` accepts only two arguments: the var name and the value. If there are more than two
    # args it is an error. The error message is verbatim from csh.
    if set -q argv[3]
        printf (_ '%s: Too many arguments\n') setenv >&2
        return 1
    end

    # We have exactly two arguments as required by the csh `setenv` command.
    set -l var $argv[1]
    set -l val $argv[2]

    # Validate the variable name.
    if not string match -qr '^\w+$' -- $var
        # This message is verbatim from csh. We don't really need to do this but if we don't fish
        # will display a different error message which might confuse someone expecting the csh
        # message.
        printf (_ '%s: Variable name must contain alphanumeric characters\n') setenv >&2
        return 1
    end

    # We need to special case some vars to be compatible with fish. In particular how they are
    # treated as arrays split on colon characters. All other var values are treated literally.
    if contains -- $var PATH CDPATH MANPATH
        set -gx $var (string split -- ':' $val)
    else
        set -gx $var $val
    end
end
