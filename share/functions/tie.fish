#
# Implement support for tying scalar and array variables together. So that updating one
# automatically updates the other. This is useful for environment variables like LD_LIBRARY_PATH.
#
# TODO: At the next major release stop special-casing PATH, MANPATH, CDPATH.

# Display the list of tied variables in a form that can be run as commands to re-tie them.
function __fish_tie_list
    # Iterate over the private vars that define tied variables. Note that we don't pipe the var
    # names through `sort` because we're relying on `set --names` output to already be sorted.
    for tied in (string match '__fish_tied_*' (set --names))
        set -l scalar_var (string replace '__fish_tied_' '' $tied)
        set -l tied_data $$tied
        set -l type $tied_data[1]

        # We pay attention to the tied vars that refer to their mirror array var and ignore the
        # inverse. This is because we only want to emit one `tie` command per pair of variables.
        if test $type != "array"
            continue
        end

        set -l array_var $tied_data[2]
        set -l sep $tied_data[3]
        printf "tie --sep %s %s %s\n" (string escape $sep) $scalar_var $array_var
    end
end

# Remove the relationship between two tied variables. This does not unset either the scalar or array
# variable. It simply stops fish from keeping them in sync. It accepts either the scalar or array
# var name.
function __fish_untie_var
    set -l var_to_untie $argv[1]
    set -l mirror_var_to_untie __fish_tied_$var_to_untie
    if not set -q $mirror_var_to_untie
        printf (_ "tie: '%s' is not a tied variable\n") $var_to_untie
        return 1
    end
    set -l mirror_var_type $$mirror_var_to_untie[1][1]
    set -l mirror_var_to_untie $$mirror_var_to_untie[1][2]

    set -e __fish_tied_$var_to_untie
    set -e __fish_tied_$mirror_var_to_untie
    if test $mirror_var_type = array
        functions -e __fish_tied_update_$mirror_var_to_untie
    else
        functions -e __fish_tied_update_$var_to_untie
    end
end

# This is a helper function that syncs two tied vars when the tie is created. It favors the scalar
# var since that is the common case. That is, tying a scalar env var like PATH to a global array
# var. If the scalar var does not exist but the array var does then the scalar var is instantiated
# as a global var.
function __fish_tie_sync_vars --no-scope-shadowing
    set -l scalar_var $argv[1]
    set -l array_var $argv[2]
    set -l sep $argv[3]

    if set -q $scalar_var
        # The scalar var in a tie has precedence. This means that if the array var is already
        # defined it is overwritten.
        __fish_tie_sync_to_array $scalar_var $array_var $sep
    else if set -q $array_var
        # If the scalar var is not defined but the array var is then set the scalar based on the
        # array var.
        __fish_tie_sync_to_scalar $scalar_var $array_var $sep
    end
end

# Sync the scalar to the array var.
function __fish_tie_sync_to_array --no-scope-shadowing
    set -l scalar_var $argv[1]
    set -l array_var $argv[2]
    set -l sep $argv[3]

    switch $scalar_var
        # TODO: In the next major update stop special casing these vars.
        case PATH MANPATH CDPATH
            if set -q $array_var
                set $array_var $$scalar_var
            else
                set -g $array_var $$scalar_var
            end
        case '*'
            if set -q $array_var
                set $array_var (string split -- $sep $$scalar_var)
            else
                set -g $array_var (string split -- $sep $$scalar_var)
            end
    end
end

# Sync the array to the scalar var.
function __fish_tie_sync_to_scalar --no-scope-shadowing
    set -l scalar_var $argv[1]
    set -l array_var $argv[2]
    set -l sep $argv[3]

    switch $scalar_var
        # TODO: In the next major update stop special casing these vars.
        case PATH MANPATH CDPATH
            if set -q $array_var
                set $scalar_var $$array_var
            else
                set -g $scalar_var $$array_var
            end
        case '*'
            if set -q $scalar_var
                set $scalar_var (string join -- $sep $$array_var)
            else
                set -gx $scalar_var (string join -- $sep $$array_var)
            end
    end
end

# Tie a scalar and array variable to each other so that changes to one are reflected in the other.
# Either of the two vars may be exported but this code does not require or assume that is the case.
# However, the common use case is to map an exported environment var to a fish array.
function __fish_tie_var --no-scope-shadowing
    set -l scalar_var $argv[1]
    set -l array_var
    if set -q argv[2]
        set array_var $argv[2]
    else
        set array_var (echo $scalar_var | tr '[:upper:]' '[:lower:]')
        if test $scalar_var = $array_var
            printf (_ "%s: You must supply two variable names when the scalar var is all lowercase\n") $cmd >&2
            return 1
        end
    end

    if test $scalar_var = $array_var
        printf (_ "%s: The scalar and array vars cannot be the same\n") $cmd >&2
        return 1
    end

    if not string match -qr '^[\w_]+$' -- $scalar_var
        printf (_ "%s: Invalid var name: %s\n") $cmd $scalar_var
        return 1
    end
    set -g __fish_tied_$array_var scalar $scalar_var $sep
    set -g __fish_tied_$scalar_var array $array_var $sep

    # Ensure the scalar and array vars are in sync at this point in time.
    __fish_tie_sync_vars $scalar_var $array_var $sep

    function __fish_tied_update_$array_var -V sep -V scalar_var -V array_var -v $scalar_var -v $array_var --no-scope-shadowing
        if set -q __fish_tied__recurse__
            return 0 # we were invoked recursively
        end

        set -l what $argv[1]
        set -l op $argv[2]
        set -l var_changed $argv[3]

        if test $what != 'VARIABLE'
            return 1 # this should be impossible
        end

        set -g __fish_tied__recurse__ 1 # make sure we don't run recursively

        if test $var_changed = $scalar_var
            # The scalar var was modified so update the tied array var.
            # Test for the unusual case of a tied var being erased.
            if test $op = ERASE
                set -e $array_var
                set -e __fish_tied__recurse__
                return 0
            end

            if test $op != SET
                set -e __fish_tied__recurse__
                return 1 # this should be impossible
            end

            # Now handle the usual case where the scalar var is being modified so we need to update
            # its mirror array var.
            __fish_tie_sync_to_array $scalar_var $array_var $sep
        else
            # The array var was modified so update the tied scalar var.
            # Test for the unusual case of a tied var being erased.
            if test $op = ERASE
                set -e $scalar_var
                set -e __fish_tied__recurse__
                return 0
            end

            if test $op != SET
                set -e __fish_tied__recurse__
                return 1 # this should be impossible
            end

            # Now handle the usual case where the array var is being modified so we need to update
            # its mirror scalar var.
            __fish_tie_sync_to_scalar $scalar_var $array_var $sep
        end

        set -e __fish_tied__recurse__
    end
end

# The `tie` command. This may list tied variables, remove a tie, or create a tie between two
# variables.
function tie
    set -l cmd $_

    if not set -q argv[1]
        __fish_tie_list
        return 0
    end

    set -l untie no
    set -l sep :
    while set -q argv[1]
        switch $argv[1]
            case -s --sep
                if not set -q argv[2]
                    printf (_ "%s: The -s / --sep option requires one argument\n") $cmd >&2
                    return 1
                end
                set sep (string sub -s 1 -l 1 -- $argv[2])
                set -e argv[2]
            case -U --untie
                set untie yes
            case -h --help
                printf (_ "usage: %s [-U] [-s SEP] SCALAR_VAR [array_var]\n") $cmd
                return 0
            case --
                set -e argv[1]
                break
            case '-*'
                printf (_ "%s: Unrecognized option %s\n") $cmd $argv[1] >&2
                return 1
            case '*'
                break
        end
        set -e argv[1]
    end

    if not set -q argv[1]
        printf (_ "%s: You must supply at least one non-option argument\n") $cmd >&2
        return 1
    else if set -q argv[2]
        and test $untie = "yes"
        printf (_ "%s: The untie option accepts only one non-option argument\n") $cmd >&2
        return 1
    else if set -q argv[3]
        printf (_ "%s: Too many non-option arguments\n") $cmd >&2
        return 1
    end

    if test $untie = "yes"
        __fish_untie_var $argv
    else
        __fish_tie_var $argv
    end
end
