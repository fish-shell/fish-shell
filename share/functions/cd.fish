#
# Wrap the builtin cd command to maintain directory history.
#
function cd --description "Change directory"
    set -l MAX_DIR_HIST 25
    set -l options h/help L/logical P/physical
    argparse -n cd --exclusive L,P --max-args=1 $options -- $argv
    or return

    # The argparse removes the options from argv. When the option -L is uses
    # it's not necessary to do something as fish always resolves the directory
    # Like POSIX it's the default option for cd.

    # Drop the argument and use the realpath
    if set -q _flag_P
        set argv (realpath $argv)
    end

    # Skip history in subshells.
    if status --is-command-substitution
        builtin cd $argv
        return $status
    end

    # Avoid set completions.
    set -l previous $PWD

    if test "$argv" = -
        if test "$__fish_cd_direction" = next
            nextd
        else
            prevd
        end
        return $status
    end

    # allow explicit "cd ." if the mount-point became stale in the meantime
    if test "$argv" = "."
        cd "$PWD"
        return $status
    end

    builtin cd $argv
    set -l cd_status $status

    if test $cd_status -eq 0 -a "$PWD" != "$previous"
        set -q dirprev
        or set -l dirprev
        set -q dirprev[$MAX_DIR_HIST]
        and set -e dirprev[1]

        # If dirprev, dirnext, __fish_cd_direction
        # are set as universal variables, honor their scope.

        set -U -q dirprev
        and set -U -a dirprev $previous
        or set -g -a dirprev $previous

        set -U -q dirnext
        and set -U -e dirnext
        or set -e dirnext

        set -U -q __fish_cd_direction
        and set -U __fish_cd_direction prev
        or set -g __fish_cd_direction prev
    end

    return $cd_status
end
