# localization: skip(private)
if not type -q apropos
    function __fish_apropos
    end
    exit
end

if test (__fish_uname) = Darwin
    and test -x /usr/libexec/makewhatis

    set -l dir
    if test -n "$XDG_CACHE_HOME"
        set dir $XDG_CACHE_HOME/fish
    else
        set dir (getconf DARWIN_USER_CACHE_DIR)"fish"
    end

    function __fish_apropos -V dir
        # macOS has a read only filesystem where the whatis database should be.

        if functions -q apropos || test "$(command -v apropos)" != /usr/bin/apropos
            __fish_without_manpager apropos "$argv"
            return
        end

        # The whatis database is non-existent, so apropos tries (and fails) to create it every time,
        # which can take seconds.
        #
        # Instead, we build a whatis database in the user cache directory
        # and override the MANPATH using that directory before we run `apropos`
        #
        # the cache is rebuilt once a week.
        set -l whatis $dir/whatis
        set -l max_age 600000 # like a week
        set -l age $max_age

        if test -f "$whatis"
            set age (path mtime -R -- $whatis)
        end

        MANPATH="$dir" __fish_without_manpager /usr/bin/apropos "$argv"

        if test $age -ge $max_age
            test -d "$dir" || mkdir -m 700 -p $dir
            /bin/sh -c '( "$@" ) >/dev/null 2>&1 </dev/null &' -- \
                /usr/libexec/makewhatis -o "$whatis" \
                (/usr/bin/manpath | string split : | xargs realpath)
        end
    end
else
    function __fish_apropos
        # we only ever prefix match for completions. This also ensures results for bare apropos <TAB>
        # (apropos '' gives no results, but apropos '^' lists all manpages)
        __fish_without_manpager apropos "$argv"
    end
end
