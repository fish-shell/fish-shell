# Support the usual (i.e., bash compatible) `umask` UI. This reports or modifies the magic global
# `umask` variable which is monitored by the fish process.

# This table is indexed by the base umask value to be modified. Each digit represents the new umask
# value when the permissions to add are applied to the base umask value.
set __fish_umask_add_table 0101010 2002200 2103210 4440000 4541010 6442200 6543210

function __fish_umask_add
    set -l mask_digit $argv[1]
    set -l to_add $argv[2]

    set -l mask_table 0000000
    if test $mask_digit -gt 0
        set mask_table $__fish_umask_add_table[$mask_digit]
    end
    set -l new_vals (string split '' $mask_table)
    echo $new_vals[$to_add]
end

# This table is indexed by the base umask value to be modified. Each digit represents the new umask
# value when the permissions to remove are applied to the base umask value.
set __fish_umask_remove_table 1335577 3236767 3337777 5674567 5775577 7676767 7777777

function __fish_umask_remove
    set -l mask_digit $argv[1]
    set -l to_remove $argv[2]

    set -l mask_table 1234567
    if test $mask_digit -gt 0
        set mask_table $__fish_umask_remove_table[$mask_digit]
    end
    set -l new_vals (string split '' $mask_table)
    echo $new_vals[$to_remove]
end

# This returns the mask corresponding to allowing the permissions to allow. In other words it
# returns the inverse of the mask passed in.
set __fish_umask_set_table 6 5 4 3 2 1 0
function __fish_umask_set
    set -l to_set $argv[1]
    if test $to_set -eq 0
        echo 7
        return
    end
    echo $__fish_umask_set_table[$to_set]
end

# Given a umask string, possibly in symbolic mode, return an octal value with leading zeros.
# This function expects to be called with a single value.
function __fish_umask_parse
    # Test if already a valid octal mask. If so pad it with zeros and return it.
    # Note that umask values are always base 8 so they don't require a leading zero.
    if string match -qr '^0?[0-7]{1,3}$' -- $argv
        string sub -s -4 0000$argv
        return 0
    end

    # Test if argument is a valid symbolic mask. Note that the basic pattern allows one illegal
    # pattern: who and perms without a mode such as "urw". We test for that below after using the
    # pattern to split the rights then testing for that invalid combination.
    set -l basic_pattern '([ugoa]*)([=+-]?)([rwx]*)'
    if not string match -qr "^$basic_pattern(,$basic_pattern)*\$" -- $argv
        printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
        return 1
    end

    # Split umask into individual digits. We erase the first one because it should always be zero.
    set -l res (string split '' $umask)
    set -e res[1]

    for rights in (string split , $argv)
        set -l match (string match -r "^$basic_pattern\$" $rights)
        set -l scope $match[2]
        set -l mode $match[3]
        set -l perms $match[4]
        if test -n "$scope" -a -z "$mode"
            printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
            return 1
        end
        if test -z "$scope"
            set scope a
        end
        if test -z "$mode"
            set mode =
        end

        set -l scopes_to_modify
        string match -q '*u*' $scope
        and set scopes_to_modify 1
        string match -q '*g*' $scope
        and set -a scopes_to_modify 2
        string match -q '*o*' $scope
        and set -a scopes_to_modify 3
        string match -q '*a*' $scope
        and set scopes_to_modify 1 2 3

        set -l val 0
        if string match -q '*r*' $perms
            set val 4
        end
        if string match -q '*w*' $perms
            set val (math $val + 2)
        end
        if string match -q '*x*' $perms
            set val (math $val + 1)
        end

        for j in $scopes_to_modify
            switch $mode
                case '='
                    set res[$j] (__fish_umask_set $val)

                case '+'
                    set res[$j] (__fish_umask_add $res[$j] $val)

                case -
                    set res[$j] (__fish_umask_remove $res[$j] $val)
            end
        end
    end

    echo 0$res[1]$res[2]$res[3]
    return 0
end

function __fish_umask_print_symbolic
    set -l val
    set -l res ""
    set -l letter a u g o

    for i in 2 3 4
        set res $res,$letter[$i]=
        set val (echo $umask|cut -c $i)

        if contains $val 0 1 2 3
            set res {$res}r
        end

        if contains $val 0 1 4 5
            set res {$res}w
        end

        if contains $val 0 2 4 6
            set res {$res}x
        end

    end

    echo (string split -m 1 '' -- $res)[2]
end

function umask --description "Set default file permission mask"
    set -l options h/help p/as-command S/symbolic
    argparse -n umask $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help umask
        return 0
    end

    switch (count $argv)
        case 0
            set -q umask
            or set -g umask 113

            if set -q _flag_as_command
                echo umask $umask
            else if set -q _flag_symbolic
                __fish_umask_print_symbolic $umask
            else
                echo $umask
            end

        case 1
            if set -l parsed (__fish_umask_parse $argv)
                set -g umask $parsed
                return 0
            end
            return 1

        case '*'
            printf (_ '%s: Too many arguments\n') umask >&2
            return 1
    end
end
