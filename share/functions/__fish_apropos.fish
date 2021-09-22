if not type -q apropos
    function __fish_apropos
    end
    exit
end

# Check for macOS Catalina or above. This is Darwin 19.x or above. See unames reported here:
# https://en.wikipedia.org/wiki/Darwin_(operating_system)
set -l sysver (uname -sr | string match -r "(Darwin) (\d\d)"\.)

if test $status -eq 0 -a (count $sysver) -eq 3
    and test $sysver[2] = Darwin -a $sysver[3] -ge 19
    and test -x /usr/libexec/makewhatis

    set -l dir
    if test -n "$XDG_CACHE_HOME"
        set dir $XDG_CACHE_HOME/fish
    else
        set dir (getconf DARWIN_USER_CACHE_DIR)"fish"
    end

    function __fish_apropos -V dir
        # macOS 10.15 "Catalina" has a read only filesystem where the whatis database should be.
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
            # Some people use GNU tools on macOS, and GNU stat works differently.
            # However it's currently guaranteed that the macOS stat is in /usr/bin,
            # so we use that explicitly.
            set age (math (date +%s) - (/usr/bin/stat -f %m $whatis))
        end

        MANPATH="$dir" apropos "^$argv"

        if test $age -ge $max_age
            test -d "$dir" || mkdir -m 700 -p $dir
            /usr/libexec/makewhatis -o "$whatis" (man --path | string split :) >/dev/null 2>&1 </dev/null &
            disown $last_pid
        end
    end
else
    function __fish_apropos
        apropos $argv
    end
end
