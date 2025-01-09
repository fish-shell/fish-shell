# This function is typically bound to Alt-W, it is used to list man page entries
# for the command under the cursor.
function __fish_whatis_current_token -d "Show man page entries or function description related to the token under the cursor"
    set -l token (commandline -pt)

    test -n "$token"
    or return

    set -l desc "$token: nothing appropriate."

    set -l tokentype (type --type $token 2>/dev/null)

    switch "$tokentype"
        case function
            set -l funcinfo (functions $token --details --verbose)

            test $funcinfo[5] != n/a
            and set desc "$token - $funcinfo[5]"

        case builtin
            set desc (__fish_print_help $token | awk "/./ { getline; print; exit }" | string trim)

        case file
            set -l tmpdesc (whatis $token 2>/dev/null)
            and set desc $tmpdesc
    end

    __fish_echo string join \n -- $desc
end
