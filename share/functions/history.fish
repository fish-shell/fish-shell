#
# Wrap the builtin history command to provide additional functionality.
#
function history --shadow-builtin --description "display or manipulate interactive command history"
    set -l cmd
    set -l search_mode --contains
    set -l with_time

    # The "set cmd $cmd xyz" lines are to make it easy to detect if the user specifies more than one
    # subcommand.
    while set -q argv[1]
        switch $argv[1]
            case -d --delete
                set cmd $cmd delete
            case -v --save
                set cmd $cmd save
            case -l --clear
                set cmd $cmd clear
            case -s --search
                set cmd $cmd search
            case -m --merge
                set cmd $cmd merge
            case -h --help
                set cmd $cmd help
            case -t --with-time
                set with_time -t
            case -p --prefix
                set search_mode --prefix
            case -c --contains
                set search_mode --contains
            case --
                set -e argv[1]
                break
            case '*'
                break
        end
        set -e argv[1]
    end

    if not set -q cmd[1]
        set cmd search  # default to "search" if the user didn't explicitly specify a command
    else if set -q cmd[2]
        printf (_ "You cannot specify multiple commands: %s\n") "$cmd"
        return 1
    end

    switch $cmd
        case search
            if isatty stdout
                set -l pager less
                set -q PAGER
                and set pager $PAGER
                builtin history --search $search_mode $with_time -- $argv | eval $pager
            else
                builtin history --search $search_mode $with_time -- $argv
            end

        case delete  # Interactively delete history
            # TODO: Fix this to deal with history entries that have multiple lines.
            if not set -q argv[1]
                printf "You have to specify at least one search term to find entries to delete" >&2
                return 1
            end

            # TODO: Fix this so that requesting history entries with a timestamp works:
            #   set -l found_items (builtin history --search $search_mode $with_time -- $argv)
            set -l found_items (builtin history --search $search_mode -- $argv)
            if set -q found_items[1]
                set -l found_items_count (count $found_items)
                for i in (seq $found_items_count)
                    printf "[%s] %s\n" $i $found_items[$i]
                end
                echo ""
                echo "Enter nothing to cancel the delete, or"
                echo "Enter one or more of the entry IDs separated by a space, or"
                echo "Enter \"all\" to delete all the matching entries."
                echo ""
                read --local --prompt "echo 'Delete which entries? > '" choice
                echo ''

                if test -z "$choice"
                    printf "Cancelling the delete!\n"
                    return
                end

                if test "$choice" = "all"
                    printf "Deleting all matching entries!\n"
                    builtin history --delete $search_mode -- $argv
                    builtin history --save
                    return
                end

                for i in (string split " " -- $choice)
                    if test -z "$i"
                    or not string match -qr '^[1-9][0-9]*$' -- $i
                    or test $i -gt $found_items_count
                        printf "Ignoring invalid history entry ID \"%s\"\n" $i
                        continue
                    end

                    printf "Deleting history entry %s: \"%s\"\n" $i $found_items[$i]
                    builtin history --delete "$found_items[$i]"
                end
                builtin history --save
            end

        case save
            builtin history --save -- $argv

        case merge
            builtin history --merge -- $argv

        case help
            builtin history --help

        case clear
            # Erase the entire history.
            read --local --prompt "echo 'Are you sure you want to clear history? (y/n) '" choice
            if test "$choice" = "y"
            or test "$choice" = "yes"
                builtin history --clear -- $argv
                and echo "History cleared!"
            end
    end
end
