function __fish_anypager --description "Print a pager to use"
    set -l pager
    # We prefer $PAGER if we have it
    set -q PAGER
    and echo $PAGER | read -at pager

    # or even $MANPAGER if we're allowed to
    if test "$argv[1]" = --with-manpager
        set -q MANPAGER
        and echo $MANPAGER | read -at pager
    end

    # We use them if they *exist*
    if command -q $pager[1]
        printf %s\n $pager
        return 0
    end

    # Cheesy hardcoded list of pagers.
    for cmd in less lv most more
        if command -q $cmd
            echo -- $cmd
            return 0
        end
    end

    # We have no pager.
    # We could fall back to "cat",
    # but in some cases that's probably not helpful.
    return 1
end
