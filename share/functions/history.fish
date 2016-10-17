#
# Wrap the builtin history command to provide additional functionality.
#

# This function is meant to mimic the `set_hist_cmd` function in *src/builtin.cpp*.
# In particular the error message should be identical in both locations.
function __fish_set_hist_cmd --no-scope-shadowing
    if set -q hist_cmd[1]
        set -l msg (printf (_ "you cannot do both '%ls' and '%ls' in the same invocation") \
            $hist_cmd $argv[1])
        printf (_ "%ls: Invalid combination of options,\n%ls\n") $cmd $msg >&2
        return 1
    end
    set hist_cmd $argv[1]
    return 0
end

function __fish_unexpected_hist_args --no-scope-shadowing
    if test -n "$search_mode"
        or test -n "$show_time"
        printf (_ "%ls: you cannot use any options with the %ls command\n") $cmd $hist_cmd >&2
        return 0
    end
    if set -q argv[1]
        printf (_ "%ls: %ls expected %d args, got %d\n") $cmd $hist_cmd 0 (count $argv) >&2
        return 0
    end
    return 1
end

function history --description "display or manipulate interactive command history"
    set -l cmd $_
    set -l cmd history
    set -l hist_cmd
    set -l search_mode
    set -l show_time
    set -l max_count
    set -l case_sensitive
    set -l null

    # Check for a recognized subcommand as the first argument.
    if set -q argv[1]
        and not string match -q -- '-*' $argv[1]
        switch $argv[1]
            case search delete merge save clear
                set hist_cmd $argv[1]
                set -e argv[1]
        end
    end

    # The "set cmd $cmd xyz" lines are to make it easy to detect if the user specifies more than one
    # subcommand.
    #
    # TODO: Remove the long options that correspond to subcommands (e.g., '--delete') on or after
    # 2017-10 (which will be a full year after these flags have been deprecated).
    while set -q argv[1]
        switch $argv[1]
            case --delete
                __fish_set_hist_cmd delete
                or return
            case --save
                __fish_set_hist_cmd save
                or return
            case --clear
                __fish_set_hist_cmd clear
                or return
            case --search
                __fish_set_hist_cmd search
                or return
            case --merge
                __fish_set_hist_cmd merge
                or return
            case -C --case_sensitive
                set case_sensitive --case-sensitive
            case -h --help
                builtin history --help
                return
            case -t --show-time '--show-time=*' --with-time '--with-time=*'
                set show_time $argv[1]
            case -p --prefix
                set search_mode --prefix
            case -c --contains
                set search_mode --contains
            case -e --exact
                set search_mode --exact
            case -z --null
                set null --null
            case -n --max
                if string match -- '-n?*' $argv[1]
                    or string match -- '--max=*' $argv[1]
                    set max_count $argv[1]
                else
                    set max_count $argv[1] $argv[2]
                    set -e argv[1]
                end
            case --
                set -e argv[1]
                break
            case '*'
                if string match -r -- '-\d+' $argv[1]
                    set max_count $argv[1]
                    set -e argv[1]
                else
                    break
                end
        end
        set -e argv[1]
    end

    # If a history command has not already been specified check the first non-flag argument for a
    # command. This allows the flags to appear before or after the subcommand.
    if not set -q hist_cmd[1]
        and set -q argv[1]
        switch $argv[1]
            case search delete merge save clear
                set hist_cmd $argv[1]
                set -e argv[1]
        end
    end

    if not set -q hist_cmd[1]
        set hist_cmd search # default to "search" if the user didn't specify a subcommand
    end

    switch $hist_cmd
        case search # search the interactive command history
            test -z "$search_mode"
            and set search_mode "--contains"

            if isatty stdout
                set -l pager less
                set -q PAGER
                and set pager $PAGER
                builtin history search $search_mode $show_time $max_count $case_sensitive $null -- $argv | eval $pager
            else
                builtin history search $search_mode $show_time $max_count $case_sensitive $null -- $argv
            end

        case delete # interactively delete history
            # TODO: Fix this to deal with history entries that have multiple lines.
            if not set -q argv[1]
                printf (_ "You must specify at least one search term when deleting entries\n") >&2
                return 1
            end

            test -z "$search_mode"
            and set search_mode "--contains"

            if test $search_mode = "--exact"
                builtin history delete $search_mode $case_sensitive $argv
                return
            end

            # TODO: Fix this so that requesting history entries with a timestamp works:
            #   set -l found_items (builtin history search $search_mode $show_time -- $argv)
            set -l found_items
            builtin history search $search_mode $case_sensitive --null -- $argv | while read -lz x
                set found_items $found_items $x
            end
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
                    for item in $found_items
                        builtin history delete --exact --case-sensitive -- $item
                    end
                    builtin history save
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
                    builtin history delete --exact --case-sensitive -- "$found_items[$i]"
                end
                builtin history save
            end

        case save # save our interactive command history to the persistent history
            __fish_unexpected_hist_args $argv
            and return 1

            builtin history save -- $argv

        case merge # merge the persistent interactive command history with our history
            __fish_unexpected_hist_args $argv
            and return 1

            builtin history merge -- $argv

        case clear # clear the interactive command history
            __fish_unexpected_hist_args $argv
            and return 1

            printf (_ "If you enter 'yes' your entire interactive command history will be erased\n")
            read --local --prompt "echo 'Are you sure you want to clear history? (yes/no) '" choice
            if test "$choice" = "yes"
                builtin history clear -- $argv
                and printf (_ "Command history cleared!")
            else
                printf (_ "You did not say 'yes' so I will not clear your command history\n")
            end

        case '*'
            printf "%ls: unexpected subcommand '%ls'\n" $cmd $hist_cmd
            return 2
    end
end
