function funcsave --description "Save the current definition of all specified functions to file"
    set -l options q/quiet h/help d/directory=
    argparse -n funcsave $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help funcsave
        return 0
    end

    set -l funcdir
    if set -q _flag_directory
        set funcdir $_flag_directory
    else
        set funcdir $__fish_config_dir/functions
    end

    if not set -q argv[1]
        printf (_ "%ls: Expected at least %d args, got only %d\n") funcsave 1 0 >&2
        return 1
    end

    if not mkdir -p $funcdir
        printf (_ "%s: Could not create configuration directory '%s'\n") funcsave $funcdir >&2
        return 1
    end

    set -l retval 0
    for funcname in $argv
        set -l funcpath "$funcdir/$funcname.fish"
        if functions -q -- $funcname
            functions --no-details -- $funcname >$funcpath
            and set -q _flag_quiet || printf (_ "%s: wrote %s\n") funcsave $funcpath
        else if test -w $funcpath
            rm $funcpath
            and set -q _flag_quiet || printf (_ "%s: removed %s\n") funcsave $funcpath
        else
            printf (_ "%s: Unknown function '%s'\n") funcsave $funcname >&2
            set retval 1
        end
    end

    return $retval
end
