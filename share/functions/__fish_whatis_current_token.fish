# This function is typically bound to Alt-W, it is used to list man page entries
# for the command under the cursor.
function __fish_whatis_current_token -d "Show man page entries or function description related to the token under the cursor"
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
            set desc (__fish_print_help $token | awk "/./ {print; exit}")

        case file
            set -l tmpdesc (whatis $token 2>/dev/null)
            and set desc $tmpdesc
    end

    printf "%s\n" $desc

    set -l line_count (count (fish_prompt))
    # Ensure line_count is greater than one to accomodate different
    # versions of the `seq` command, some of which print the sequence in
    # reverse order when the second argument is smaller than the first
    if test $line_count -gt 1
        for x in (seq 2 $line_count)
            printf "\n"
        end
    end

    commandline -f repaint
end
