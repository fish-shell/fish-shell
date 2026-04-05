function cygwin_nosymlinks --description \
    "Return 0 if Cygwin fakes symlinks, return 1 otherwise"

    switch (__fish_uname)
        case "MSYS*"
            not string match -q "*winsymlinks*" -- "$MSYS"
        case "CYGWIN*"
            not string match -q "*winsymlinks*" -- "$CYGWIN"
        case "*"
            return 1
    end
end
