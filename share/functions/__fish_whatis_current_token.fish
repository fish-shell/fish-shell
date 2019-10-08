# This function is typically bound to Alt-W, it is used to list man page entries
# for the command under the cursor.
function __fish_whatis_current_token -d "Show man page entries related to the token under the cursor"
    set -l tok (commandline -pt)

    if test -n "$tok[1]"
        printf "\n"
        whatis $tok[1]

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
