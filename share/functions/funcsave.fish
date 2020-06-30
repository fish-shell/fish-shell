function funcsave --description "Save the current definition of all specified functions to file"
    set -l options h/help 'd/directory='
    argparse -n funcsave $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help funcsave
        return 0
    end

    if set -q _flag_directory
        set funcdir $_flag_directory
    else
        set funcdir $__fish_config_dir/functions
    end

    if not set -q argv[1]
        printf (_ "%ls: Expected at least %d args, got only %d\n") funcsave 1 0
        return 1
    end

    if not mkdir -p $funcdir
        printf (_ "%s: Could not create configuration directory '%s'\n") funcsave $funcdir
        return 1
    end

    set -l retval 0
    for funcname in $argv
        if functions -q -- $funcname
            functions -- $funcname >$funcdir/$funcname.fish
        else
            printf (_ "%s: Unknown function '%s'\n") funcsave $funcname
            set retval 1
        end
    end

    return $retval
end

