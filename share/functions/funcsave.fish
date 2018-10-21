function funcsave --description "Save the current definition of all specified functions to file"
    set -l options 'h/help'
    argparse -n funcsave --min-args=1 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help funcsave
        return 0
    end

    set -l configdir ~/.config
    if set -q XDG_CONFIG_HOME
        set configdir $XDG_CONFIG_HOME
    end

    if not mkdir -p $configdir/fish/functions
        printf (_ "%s: Could not create configuration directory\n") funcsave
        return 1
    end

    set -l retval 0
    for funcname in $argv
        if functions -q -- $funcname
            functions -- $funcname >$configdir/fish/functions/$funcname.fish
        else
            printf (_ "%s: Unknown function '%s'\n") funcsave $funcname
            set retval 1
        end
    end

    return $retval
end

