function cygwin_noacl --description \
    "Return 0 if a path does not support POSIX file permissions on Cygwin/MSYS,
    return 1 otherwise"

    set -q argv[1] || { echo "cygwin_noacl: missing path" >&2; exit 2 }

    __fish_cygwin_noacl $argv[1]
end
