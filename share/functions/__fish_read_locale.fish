# Try to set the locale from the system configuration if we did not inherit any.
# One case where this can happen is a linux with systemd, used via getty (and then possibly startx).
# See https://github.com/fish-shell/fish-shell/issues/3092.
# This isn't actually our job, so there's a bunch of edge-cases we are unlikely to handle properly.
# If we get a value for _any_ language variable, we assume we've inherited something sensible so we skip this
# to allow the user to set it at runtime without mucking with config files.
# NOTE: This breaks the expectation that an empty LANG will be the same as LANG=POSIX,
# but an empty LANG seems more likely to be caused by mis- or missing configuration.

# Do this before reading the snippets because they might be in UTF-8 (with non-ASCII characters).
function __fish_read_locale
    # Note the missing LC_ALL - locale.conf doesn't allow it, so we should not set it.
    set -l LANGVARS LANG LANGUAGE LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY LC_MESSAGES LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT LC_IDENTIFICATION

    # We use LC_ALL to figure out if we're without a locale, but we don't set it later.
    if string length -q -- $$LANGVARS $LC_ALL
        return 0
    end

    # Unset all variables - they are empty anyway and this makes merging easier.
    for v in $LANGVARS
        set -e $v
    end

    # First read from the kernel commandline.
    # The splitting here is a bit weird, but we operate under the assumption that the locale can't include whitespace.
    # Other whitespace shouldn't concern us, but a quoted "locale.LANG=SOMETHING" as a value to something else might.
    # Here the last definition of a variable takes precedence.
    if test -r /proc/cmdline
        for var in (string match -ra 'locale.[^=]+=\S+' < /proc/cmdline)
            set -l kv (string replace 'locale.' '' -- $var | string split '=')
            # Only set locale variables, not other stuff contained in these files - this also automatically ignores comments.
            if contains -- $kv[1] $LANGVARS; and set -q kv[2]
                set -gx $kv[1] (string trim -c '\'"' -- $kv[2])
            end
        end
    end

    # Now read the config files.
    # /etc/sysconfig/i18n is for old Red Hat derivatives (and possibly of no use anymore).
    # /etc/env.d/02locale is from OpenRC, the rest are systemd inventions but also used elsewhere (e.g. Void Linux).
    # systemd's documentation is a bit unclear on this. We merge all the config files (and the commandline),
    # which seems to be what systemd itself does. (I.e. the value for a variable will be taken from the highest-precedence source)
    # We read the systemd files first since they are a newer invention and therefore the rest are likely to be accumulated cruft.
    # NOTE: Slackware expects locale in /etc/profile.d/lang.sh, which we can't use because it's a full POSIX-shell script.
    for f in (set -q XDG_CONFIG_HOME; and echo $XDG_CONFIG_HOME; or echo ~/.config)/locale.conf /etc/locale.conf /etc/env.d/02locale /etc/sysconfig/i18n
        if test -r $f
            while read -l kv
                set kv (string split '=' -- $kv)
                if contains -- $kv[1] $LANGVARS; and set -q kv[2]
                    # Do not set already set variables again - this makes the merging happen.
                    if not set -q $kv[1]
                        set -gx $kv[1] (string trim -c '\'"' -- $kv[2])
                    end
                end
            end < $f
        end
    end

    # If we really cannot get anything, at least set encoding to UTF-8.
    if not string length -q -- $$LANGVARS $LC_ALL
        set LC_CTYPE en_US.UTF-8
    end
end
