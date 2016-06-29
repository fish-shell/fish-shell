#
# Wrap the builtin history command to provide additional functionality.
#
function history --shadow-builtin --description "display or manipulate interactive command history"
    if not set -q argv[1]
        # No arguments so execute history builtin using it's default behavior to display the entire
        # history.
        if status --is-interactive
	    set -l pager less
	    set -q PAGER
	    and set pager $PAGER
            builtin history --with-time | eval $pager
        else
            builtin history
        end
        return
    end

    set -l cmd search
    set -l prefix_args ""
    set -l contains_args ""
    set -l search_mode none
    set -l time_args

    for i in (seq (count $argv))
	switch $argv[$i]
	    case -d --delete
		set cmd delete
	    case -v --save
		set cmd save
	    case -l --clear
		set cmd clear
	    case -s --search
		set cmd search
	    case -m --merge
		set cmd merge
	    case -h --help
		set cmd help
	    case -t --with-time
		set time_args --with-time
	    case -p --prefix
		set search_mode prefix
		set prefix_args $argv[(math $i + 1)]
	    case -c --contains
		set search_mode contains
		set contains_args $argv[(math $i + 1)]
	    case --
		set -e argv[1..$i]
		break
	    case "-*" "--*"
		printf ( _ "%s: invalid option -- %s\n" ) history $argv[$i] >&2
		return 1
	end
    end

    switch $cmd
        case search
            builtin history $time_args --search $argv

        case delete
            # Interactively delete history
            set -l found_items ""
            switch $search_mode
                case prefix
                    set found_items (builtin history --search --prefix $prefix_args)
                case contains
                    set found_items (builtin history --search --contains $contains_args)
                case none
                    builtin history $argv
                    # Save changes after deleting item.
                    builtin history --save
                    return 0
            end

            set found_items_count (count $found_items)
            if test $found_items_count -gt 0
                echo "[0] cancel"
                echo "[1] all"
                echo

                for i in (seq $found_items_count)
                    printf "[%s] %s \n" (math $i + 1) $found_items[$i]
                end

                read --local --prompt "echo 'Delete which entries? > '" choice
                set choice (string split " " -- $choice)

                for i in $choice

                    # Skip empty input, for example, if the user just hits return
                    if test -z $i
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
            # Save changes to history file.
            builtin history $argv
        case merge
            builtin history --merge
        case help
            builtin history --help
        case clear
            # Erase the entire history.
            echo "Are you sure you want to clear history ? (y/n)"
            read ch
            if test $ch = "y"
                builtin history $argv
                echo "History cleared!"
            end
    end
end
