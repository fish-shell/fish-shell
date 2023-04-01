# This function is typically bound to Alt-W, it is used to list man page entries
# for the command under the cursor.
function __ghoti_whatis_current_token -d "Show man page entries or function description related to the token under the cursor"
    set -l token (commandline -pt)

    test -n "$token"
    or return

    printf "\n"
    set -l desc "$token: nothing appropriate."

    set -l tokentype (type --type $token 2>/dev/null)

    switch "$tokentype"
        case function
            set -l funcinfo (functions $token --details --verbose)

            test $funcinfo[5] != n/a
            and set desc "$token - $funcinfo[5]"

        case builtin
            set desc (__ghoti_print_help $token | awk "/./ {print; exit}")

        case file
            set -l tmpdesc (whatis $token 2>/dev/null)
            and set desc $tmpdesc
    end

    printf "%s\n" $desc

    string repeat -N \n --count=(math (count (ghoti_prompt)) - 1)

    commandline -f repaint
end
