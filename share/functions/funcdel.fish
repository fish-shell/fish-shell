function funcdel --description "Remove the current definition of all specified functions from session and file"
    set -l options q/quiet h/help d/directory=
    argparse -n funcdel $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help funcdel
        return 0
    end

    if set -q _flag_directory
        set funcdir $_flag_directory
    else
        set funcdir $fish_function_path
    end

    if not set -q argv[1]
        printf (_ "%ls: Expected at least %d args, got only %d\n") funcdel 1 0 >&2
        return 1
    end

    if not test -d $funcdir
        printf (_ "%s: Configuration directory does not exist '%s'\n") funcdel $funcdir >&2
        return 1
    end

    set -l retval 0
    for funcname in $argv
        set -l funcpath "$funcdir/$funcname.fish"

        if not functions -q -- $funcname
            printf (_ "%s: not defined as a function '%s'\n") funcdel $funcname >&2
            set retval 1
        else if not test -e $funcpath
            echo "$funcname does not exist as a user function"
            printf (_ "%s: not a saved user function '%s'\n") funcdel $funcname >&2
            set retval 2
        else
            functions --erase -- $funcname
            rm $funcpath
            and set -q _flag_quiet || printf (_ "%s: removed %s\n") funcdel $funcpath
        end
    end

    return $retval
end
