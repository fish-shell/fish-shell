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
            if not set -q argv[1]
                echo "You have to specify at least one search term to find entries to delete" 2>&1
                return 1
            end

            set -l found_items (builtin history --search $search_mode $with_time -- $argv)
            if set -q found_items[1]
                set -l found_items_count (count $found_items)
                for i in (seq $found_items_count)
                    printf "[%s] %s \n" (math $i + 1) $found_items[$i]
                end
                echo
                echo "[0] cancel"
                echo "[1] all"

                read --local --prompt "echo 'Delete which entries? > '" choice
                set choice (string split " " -- $choice)

                for i in $choice
                    # Skip empty input, for example, if the user just hits return
                    if test -z "$i"
                        continue
                    end

                    # Following two validations could be embedded with "and" but I find the syntax
                    # kind of weird.
                    if not string match -qr '^[0-9]+$' $i
                        printf "Invalid input: %s\n" $i
                        continue
                    end

                    if test $i -gt (math $found_items_count + 1)
                        printf "Invalid input : %s\n" $i
                        continue
                    end

                    if test $i = "0"
                        printf "Cancel\n"
                        return
                    else
                        if test $i = "1"
                            for item in $found_items
                                builtin history --delete $item
                            end
                            printf "Deleted all!\n"
                        else
                            builtin history --delete $found_items[(math $i - 1)]
                        end

                    end
                end
                # Save changes after deleting item(s).
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
            echo "Are you sure you want to clear history? (y/n)"
            read ch
            if test $ch = "y"
                builtin history --clear -- $argv
                and echo "History cleared!"
            end
    end
end
