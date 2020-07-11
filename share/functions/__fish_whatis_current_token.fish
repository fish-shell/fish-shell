# This function is typically bound to Alt-W, it is used to list man page entries
# for the command under the cursor.
function __fish_whatis_current_token -d "Show man page entries or function description related to the token under the cursor"
    set -l token (commandline -pt)

    if test -n "$token"
        printf "\n"
        whatis $token 2>/dev/null

        if test $status -eq 16 # no match found
            set -l tokentype (type --type $token 2>/dev/null)
            set -l desc ": nothing appropriate."

            if test "$tokentype" = "function"
                set -l funcinfo (functions $token --details --verbose)

                test $funcinfo[5] != "n/a"
                and set desc " - $funcinfo[5]"
            end

            printf "%s%s\n" $token $desc
        end

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
end
