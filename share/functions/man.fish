# localization: skip(uses-apropos)

if not command -qs man
    # see #5329 and discussion at https://github.com/fish-shell/fish-shell/commit/13e025bdb01cc4dd08463ec497a0a3495873702f
    exit
end

function man
    set -l manpath
    if not __fish_is_standalone
        and set -l fish_manpath (path filter -d $__fish_data_dir/man)
        # Prepend fish's man directory if available.

        # Work around the "builtin" manpage that everything symlinks to,
        # by prepending our fish datadir to man. This also ensures that man gives fish's
        # man pages priority, without having to put fish's bin directories first in $PATH.

        # Preserve the existing MANPATH, and default to the system path (the empty string).
        set manpath $fish_manpath (
            if set -q MANPATH
                string join -- \n $MANPATH
            else if set -l p (command man -p 2>/dev/null)
                # NetBSD's man uses "-p" to print the path.
                # FreeBSD's man also has a "-p" option, but that requires an argument.
                # Other mans (men?) don't seem to have it.
                #
                # Unfortunately NetBSD prints things like "/usr/share/man/man1",
                # while not allowing them as $MANPATH components.
                # What it needs is just "/usr/share/man".
                #
                # So we strip the last component.
                # This leaves a few wrong directories, but that should be harmless.
                string replace -r '[^/]+$' '' $p
            else
                echo ''
            end
        )
    end

    if test (count $argv) -eq 1
        set argv (__fish_canonicalize_builtin $argv)
    end

    if not set -q argv[2] && status list-files "man/man1/$argv[1].1" &>/dev/null
        set -l basename $argv[1].1
        function __fish_man -V basename -a man1
            # mandoc man needs "-l" or it'll refuse to open a file
            command man -l $man1/$basename
        end
        __fish_data_with_directory man/man1 \
            (string escape --style=regex -- $basename) __fish_man
        __fish_with_status functions --erase __fish_man
    else
        set -q manpath[1]
        and set -lx MANPATH $manpath

        command man $argv
    end
end
