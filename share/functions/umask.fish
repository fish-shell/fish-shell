# Support the usual (i.e., bash compatible) `umask` UI. This reports or modifies the magic global
# `umask` variable which is monitored by the fish process.

# Indexed by base umask value to be modified. Each digit represents the new value when the
# corresponding index value is or'd with the base umask value.
set __fish_umask_add_table 1335577 3236767 3337777 5674567 5775577 7676767 7777777

function __fish_umask_add
    set -l val $argv[1]
    set -l to_add $argv[2]

    set -l mask_table 1234567
    if test $val -gt 0
        set mask_table $__fish_umask_add_table[$val]
    end
    set -l new_vals (string split '' $mask_table)
    echo $new_vals[$to_add]
end

# Indexed by base umask value to be modified. Each digit represents the new value when the
# corresponding index value is not and'd with the base umask value.
set __fish_umask_remove_table 0101010 2002200 2103210 4440000 4541010 6442200 6543210

function __fish_umask_remove
    set -l val $argv[1]
    set -l to_remove $argv[2]

    set -l mask_table 0000000
    if test $val -gt 0
        set mask_table $__fish_umask_remove_table[$val]
    end
    set -l new_vals (string split '' $mask_table)
    echo $new_vals[$to_remove]
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
    # pattern: who and perms without a mode such as "urw". We test for that below after splitting
    # the rights using the pattern and testing for that invalid combination.
    set -l basic_pattern '([ugoa]*)([=+-]?)([rwx]*)'
    if not string match -qr "^$basic_pattern(,$basic_pattern)*\$" -- $argv
        printf (_ "%s: Invalid mask '%s'\n") umask $argv >&2
        return 1
    end

    # Insert inverted umask into res variable.
    set -l tmp (string split '' $umask)
    set -l res
    set res[1] (math 7 - $tmp[2])
    set res[2] (math 7 - $tmp[3])
    set res[3] (math 7 - $tmp[4])

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
        and set scopes_to_modify $scopes_to_modify 2
        string match -q '*o*' $scope
        and set scopes_to_modify $scopes_to_modify 3
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
                    set res[$j] $val

                case '+'
                    set res[$j] (__fish_umask_add $res[$j] $val)

                case '-'
                    set res[$j] (__fish_umask_remove $res[$j] $val)
            end
        end
    end

    for i in 1 2 3
        set res[$i] (math 7 - $res[$i])
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
    set -l as_command 0
    set -l symbolic 0
    set -l options
    set -l shortopt pSh
    if not getopt -T >/dev/null
        # GNU getopt
        set longopt -l as-command,symbolic,help
        set options -o $shortopt $longopt --
        # Verify options
        if not getopt -n umask $options $argv >/dev/null
            return 1
        end
    else
        # Old getopt, used on OS X
        set options $shortopt
        # Verify options
        if not getopt $options $argv >/dev/null
            return 1
        end
    end

    set -l tmp (getopt $options $argv)
    eval set opt $tmp

    while count $opt >/dev/null
        switch $opt[1]
            case -h --help
                __fish_print_help umask
                return 0

            case -p --as-command
                set as_command 1

            case -S --symbolic
                set symbolic 1

            case --
                set -e opt[1]
                break
        end

        set -e opt[1]
    end

    switch (count $opt)
        case 0
            if not set -q umask
                set -g umask 113
            end
            if test $as_command -eq 1
                echo umask $umask
            else
                if test $symbolic -eq 1
                    __fish_umask_print_symbolic $umask
                else
                    echo $umask
                end
            end

        case 1
            if set -l parsed (__fish_umask_parse $opt)
                set -g umask $parsed
                return 0
            end
            return 1

        case '*'
            printf (_ '%s: Too many arguments\n') umask >&2
            return 1
    end
end
