function abbr --description "Manage abbreviations"
    set -l options --stop-nonopt --exclusive 'a,r,e,l,s,q' --exclusive 'g,U'
    set -a options h/help a/add r/rename e/erase l/list s/show q/query
    set -a options g/global U/universal

    argparse -n abbr $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help abbr
        return 0
    end

    # If run with no options, treat it like --add if we have arguments, or
    # --show if we do not have any arguments.
    set -l _flag_add
    set -l _flag_show
    if not set -q _flag_add[1]
        and not set -q _flag_rename[1]
        and not set -q _flag_erase[1]
        and not set -q _flag_list[1]
        and not set -q _flag_show[1]
        and not set -q _flag_query[1]
        if set -q argv[1]
            set _flag_add --add
        else
            set _flag_show --show
        end
    end

    set -l abbr_scope
    if set -q _flag_global
        set abbr_scope --global
    else if set -q _flag_universal
        set abbr_scope --universal
    end

    if set -q _flag_add[1]
        __fish_abbr_add $argv
        return
    else if set -q _flag_erase[1]
        __fish_abbr_erase $argv
        return
    else if set -q _flag_rename[1]
        __fish_abbr_rename $argv
        return
    else if set -q _flag_list[1]
        __fish_abbr_list $argv
        return
    else if set -q _flag_show[1]
        __fish_abbr_show $argv
        return
    else if set -q _flag_query[1]
        # "--query": Check if abbrs exist.
        # If we don't have an argument, it's an automatic failure.
        set -q argv[1]; or return 1
        set -l escaped _fish_abbr_(string escape --style=var -- $argv)
        # We return 0 if any arg exists, whereas `set -q` returns the number of undefined arguments.
        # But we should be consistent with `type -q` and `command -q`.
        for var in $escaped
            set -q $escaped; and return 0
        end
        return 1
    else
        printf ( _ "%s: Could not figure out what to do!\n" ) abbr >&2
        return 127
    end
end

function __fish_abbr_add --no-scope-shadowing
    if not set -q argv[2]
        printf ( _ "%s %s: Requires at least two arguments\n" ) abbr --add >&2
        return 1
    end

    # Because of the way abbreviations are expanded there can't be any spaces in the key.
    set -l abbr_name $argv[1]
    set -l escaped_abbr_name (string escape -- $abbr_name)
    if string match -q "* *" -- $abbr_name
        set -l msg ( _ "%s %s: Abbreviation %s cannot have spaces in the word\n" )
        printf $msg abbr --add $escaped_abbr_name >&2
        return 1
    end

    set -l abbr_val "$argv[2..-1]"
    set -l abbr_var_name _fish_abbr_(string escape --style=var -- $abbr_name)

    if not set -q $abbr_var_name
        # We default to the universal scope if the user didn't explicitly specify a scope and the
        # abbreviation isn't already defined.
        set -q abbr_scope[1]
        or set abbr_scope --universal
    end
    true # make sure the next `set` command doesn't leak the previous status
    set $abbr_scope $abbr_var_name $abbr_val
end

function __fish_abbr_erase --no-scope-shadowing
    if set -q argv[2]
        printf ( _ "%s %s: Expected one argument\n" ) abbr --erase >&2
        return 1
    end

    # Because of the way abbreviations are expanded there can't be any spaces in the key.
    set -l abbr_name $argv[1]
    set -l escaped_name (string escape -- $abbr_name)
    if string match -q "* *" -- $abbr_old_name
        set -l msg ( _ "%s %s: Abbreviation %s cannot have spaces in the word\n" )
        printf $msg abbr --erase $escaped_name >&2
        return 1
    end

    set -l abbr_var_name _fish_abbr_(string escape --style=var -- $abbr_name)

    if not set -q $abbr_var_name
        printf ( _ "%s %s: No abbreviation named %s\n" ) abbr --erase $escaped_name >&2
        return 4 # like `set -e doesnt_exist`
    end

    set -e $abbr_var_name
end

function __fish_abbr_rename --no-scope-shadowing
    if test (count $argv) -ne 2
        printf ( _ "%s %s: Requires exactly two arguments\n" ) abbr --rename >&2
        return 1
    end

    set -l old_name $argv[1]
    set -l new_name $argv[2]
    set -l escaped_old_name (string escape -- $old_name)
    set -l escaped_new_name (string escape -- $new_name)
    if string match -q "* *" -- $old_name
        set -l msg ( _ "%s %s: Abbreviation %s cannot have spaces in the word\n" )
        printf $msg abbr --rename $escaped_old_name >&2
        return 1
    end
    if string match -q "* *" -- $new_name
        set -l msg ( _ "%s %s: Abbreviation %s cannot have spaces in the word\n" )
        printf $msg abbr --rename $escaped_new_name >&2
        return 1
    end

    set -l old_var_name _fish_abbr_(string escape --style=var -- $old_name)
    set -l new_var_name _fish_abbr_(string escape --style=var -- $new_name)

    if not set -q $old_var_name
        printf ( _ "%s %s: No abbreviation named %s\n" ) abbr --rename $escaped_old_name >&2
        return 1
    end
    if set -q $new_var_name
        set -l msg ( _ "%s %s: Abbreviation %s already exists, cannot rename %s\n" )
        printf $msg abbr --rename $escaped_new_name $escaped_old_name >&2
        return 1
    end

    set -l old_var_val $$old_var_name

    if not set -q abbr_scope[1]
        # User isn't forcing the scope so use the existing scope.
        if set -ql $old_var_name
            set abbr_scope --global
        else
            set abbr_scope --universal
        end
    end

    set -e $old_var_name
    set $abbr_scope $new_var_name $old_var_val
end

function __fish_abbr_list --no-scope-shadowing
    if set -q argv[1]
        printf ( _ "%s %s: Unexpected argument -- '%s'\n" ) abbr --erase $argv[1] >&2
        return 1
    end

    for var_name in (set --names)
        string match -q '_fish_abbr_*' $var_name
        or continue

        set -l abbr_name (string unescape --style=var (string sub -s 12 $var_name))
        echo $abbr_name
    end
end

function __fish_abbr_show --no-scope-shadowing
    if set -q argv[1]
        printf ( _ "%s %s: Unexpected argument -- '%s'\n" ) abbr --erase $argv[1] >&2
        return 1
    end

    for var_name in (set --names)
        string match -q '_fish_abbr_*' $var_name
        or continue

        set -l abbr_var_name $var_name
        set -l abbr_name (string unescape --style=var -- (string sub -s 12 $abbr_var_name))
        set -l abbr_name (string escape --style=script -- $abbr_name)
        set -l abbr_val $$abbr_var_name
        set -l abbr_val (string escape --style=script -- $abbr_val)

        if set -ql $abbr_var_name
            printf 'abbr -a %s -- %s %s\n' -l $abbr_name $abbr_val
        end
        if set -qg $abbr_var_name
            printf 'abbr -a %s -- %s %s\n' -g $abbr_name $abbr_val
        end
        if set -qU $abbr_var_name
            printf 'abbr -a %s -- %s %s\n' -U $abbr_name $abbr_val
        end
    end
end
