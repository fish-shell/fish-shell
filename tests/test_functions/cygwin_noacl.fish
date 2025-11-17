function cygwin_noacl --description \
    "Return 0 if a path does not support POSIX file permissions on Cygwin/MSYS,
    return 1 otherwise"

    set -q argv[1] || { echo "cygwin_noacl: missing path" >&2; exit 2 }

    # MSYS (default) and Cygwin (non-default) mounts may not support POSIX
    # permissions. Define a `noacl` variable if that's not the case, so we can cheat
    # on the test
    is_cygwin || return 1

    mount | string match "*on $(stat -c %m -- $argv[1]) *" | string match -qr "[(,]noacl[),]"
end
