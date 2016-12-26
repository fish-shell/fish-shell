function __fish_man_page
    # Get all commandline tokens not starting with "-"
    set -l args (commandline -po | string match -rv '^-')

    # If commandline is empty, exit.
    if not set -q args[1]
        printf \a
        return
    end

    # If there are at least two tokens not starting with "-", the second one might be a subcommand.
    # Try "man first-second" and fall back to "man first" if that doesn't work out.
    set -l maincmd (basename $args[1])
    if set -q args[2]
        man "$maincmd-$args[2]" ^/dev/null
        or man "$maincmd" ^/dev/null
        or printf \a
    else
        man "$maincmd" ^/dev/null
        or printf \a
    end

    commandline -f repaint
end
