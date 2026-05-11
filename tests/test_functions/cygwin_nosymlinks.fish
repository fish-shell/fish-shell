function cygwin_nosymlinks --description \
    "Return 0 if Cygwin fakes symlinks, return 1 otherwise"

    switch (__fish_uname)
        case "CYGWIN*"
            # Cygwin has various ways of creating symlinks but they should
            # all be recognized as such (with a few exceptions, which we do
            # not support in testing, see
            # https://cygwin.com/cygwin-ug-net/using.html#pathnames-symlinks)
            return 1
        case "MSYS*"
            # In addition to the standard Cygwin symlinks, MSYS2 also has
            # `deepcopy` mode, which is the default, and does not create
            # recognizable symlinks
            # (https://www.msys2.org/docs/symlinks/)
            not string match -q "*winsymlinks*" -- "$MSYS"
            or string match -q "*winsymlinks:deepcopy*" -- "$MSYS"
        case "*"
            return 1
    end
end
