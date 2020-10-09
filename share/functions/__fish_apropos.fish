if not type -q apropos
    function __fish_apropos
    end
    exit
end

function __fish_apropos
    # macOS 10.15 "Catalina" has some major issues.
    # The whatis database is non-existent, so apropos tries (and fails) to create it every time,
    # which takes about half a second.
    #
    # Instead, we build a whatis database in the user cache directory
    # and override the MANPATH using that directory before we run `apropos`
    #
    # the cache is rebuilt once a day
    if test (uname) = Darwin
        set -l cache $HOME/.cache/fish/
        if test -n "$XDG_CACHE_HOME"
            set cache $XDG_CACHE_HOME/fish
        end

        set -l db $cache/whatis
        set -l max_age 86400 # one day
        set -l age $max_age

        if test -f "$db"
            # Some people use GNU tools on macOS, and GNU stat works differently.
            # However it's currently guaranteed that the macOS stat is in /usr/bin,
            # so we use that explicitly.
            set age (math (date +%s) - (/usr/bin/stat -f %m $db))
        end

        MANPATH="$cache" apropos $argv

        if test $age -ge $max_age
            mkdir -m 700 -p $cache
            /usr/libexec/makewhatis -o $db (man --path | string split :) >/dev/null 2>&1 </dev/null &
        end
    else
        apropos $argv
    end
end
