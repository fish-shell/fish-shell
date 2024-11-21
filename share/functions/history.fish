#
# Wrap the builtin history command to provide additional functionality.
#
function history --description "display or manipulate interactive command history"
    set -l cmd history
    set -l options --exclusive 'c,e,p' --exclusive 'S,D,M,V,X'
    set -a options h/help c/contains e/exact p/prefix
    set -a options C/case-sensitive R/reverse z/null 't/show-time=?' 'n#max'
    # The following options are deprecated and will be removed in the next major release.
    # Note that they do not have usable short flags.
    set -a options S-search D-delete M-merge V-save X-clear
    argparse -n $cmd $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help history
        return 0
    end

    set -l hist_cmd
    set -l show_time
    set -l max_count
    set -l search_mode
    set -q _flag_max
    set max_count -n$_flag_max

    set -q _flag_with_time
    and set -l _flag_show_time $_flag_with_time
    if set -q _flag_show_time[1]
        set show_time --show-time=$_flag_show_time
    else if set -q _flag_show_time
        set show_time --show-time
    end

    set -q _flag_prefix
    and set -l search_mode --prefix
    set -q _flag_contains
    and set -l search_mode --contains
    set -q _flag_exact
    and set -l search_mode --exact

    if set -q _flag_delete
        set hist_cmd delete
    else if set -q _flag_save
        set hist_cmd save
    else if set -q _flag_clear
        set hist_cmd clear
    else if set -q _flag_search
        set hist_cmd search
    else if set -q _flag_merge
        set hist_cmd merge
    else if set -q _flag_clear-session
        set hist_cmd clear-session
    end

    # If a history command has not already been specified check the first non-flag argument for a
    # command. This allows the flags to appear before or after the subcommand.
    if not set -q hist_cmd[1]
        and set -q argv[1]
        if contains $argv[1] search delete merge save clear clear-session append
            set hist_cmd $argv[1]
            set -e argv[1]
        end
    end

    if not set -q hist_cmd[1]
        set hist_cmd search # default to "search" if the user didn't specify a subcommand
    end

    switch $hist_cmd
        case search # search the interactive command history
            if isatty stdout
                set -l pager (__fish_anypager)
                and isatty stdout
                or set pager cat

                # If the user hasn't preconfigured less with the $LESS environment variable,
                # we do so to have it behave like cat if output fits on one screen.
                if not set -qx LESS
                    set -fx LESS --quit-if-one-screen
                    # Also set --no-init for less < v530, see #8157.
                    if type -q less; and test (less --version | string match -r 'less (\d+)')[2] -lt 530 2>/dev/null
                        set LESS $LESS --no-init
                    end
                end
                not set -qx LV # ask the pager lv not to strip colors
                and set -fx LV -c

                builtin history search $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv | $pager
            else
                builtin history search $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv
            end

        case delete # interactively delete history
            set -l searchterm $argv
            if not set -q argv[1]
                read -P"Search term: " searchterm
            end

            if test "$search_mode" = --exact
                builtin history delete $search_mode $_flag_case_sensitive -- $searchterm
                builtin history save
                return
            end

            # TODO: Fix this so that requesting history entries with a timestamp works:
            #   set -l found_items (builtin history search $search_mode $show_time -- $argv)
            set -l found_items
            set found_items (builtin history search $search_mode $_flag_case_sensitive --null -- $searchterm | string split0)
            if set -q found_items[1]
                set -l found_items_count (count $found_items)
                for i in (seq $found_items_count)
                    printf "[%s] %s\n" $i $found_items[$i]
                end
                echo
                echo "Enter nothing to cancel the delete, or"
                echo "Enter one or more of the entry IDs or ranges like '5..12', separated by a space."
                echo "For example '7 10..15 35 788..812'."
                echo "Enter 'all' to delete all the matching entries."
                echo
                read --local --prompt "echo 'Delete which entries? '" choice
                echo ''

                if test -z "$choice"
                    printf "Cancelling the delete!\n"
                    return
                end

                if test "$choice" = all
                    printf "Deleting all matching entries!\n"
                    for item in $found_items
                        builtin history delete --exact --case-sensitive -- $item
                    end
                    builtin history save
                    return
                end

                set -l choices
                for i in (string split " " -- $choice)
                    # Expand ranges like 577..580
                    if set -l inds (string match -rg '^([1-9][0-9]*)\.\.([1-9][0-9]*)' -- $i)
                        if test $inds[1] -gt $found_items_count
                            or test $inds[1] -gt $inds[2]
                            printf (_ "Ignoring invalid history entry ID \"%s\"\n") $i
                            continue
                        end

                        set -l indexes (seq $inds[1] 1 $inds[2])
                        if set -q indexes[1]
                            set -a choices $indexes
                        end
                        continue
                    end

                    if string match -qr '^[1-9][0-9]*$' -- $i
                        and test $i -le $found_items_count
                        set -a choices $i
                    else
                        printf (_ "Ignoring invalid history entry ID \"%s\"\n") $i
                        continue
                    end
                end

                if not set -q choices[1]
                    return 1
                end

                set choices (path sort -u -- $choices)

                echo Deleting choices: $choices
                for x in $choices
                    printf "Deleting history entry %s: \"%s\"\n" $x $found_items[$x]
                    builtin history delete --exact --case-sensitive -- "$found_items[$x]"
                end
                builtin history save
            end

        case save # save our interactive command history to the persistent history
            builtin history save $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv
        case merge # merge the persistent interactive command history with our history
            builtin history merge $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv
        case clear # clear the interactive command history
            if test -n "$search_mode"
                or set -q show_time[1]
                printf (_ "%ls: %ls: subcommand takes no options\n") history $hist_cmd >&2
                return 1
            end
            if set -q argv[1]
                printf (_ "%ls: %ls: expected %d arguments; got %d\n") history $hist_cmd 0 (count $argv) >&2
                return 1
            end

            printf (_ "If you enter 'yes' your entire interactive command history will be erased\n")
            read --local --prompt "echo 'Are you sure you want to clear history? (yes/no) '" choice
            if test "$choice" = yes
                builtin history clear $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv
                and printf (_ "Command history cleared!\n")
            else
                printf (_ "You did not say 'yes' so I will not clear your command history\n")
            end
        case clear-session # clears only session
            builtin history clear-session $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $argv
            and printf (_ "Command history for session cleared!\n")
        case append
            set -l newitem $argv
            if not set -q argv[1]
                read -P "Command: " newitem
            end

            builtin history append $search_mode $show_time $max_count $_flag_case_sensitive $_flag_reverse $_flag_null -- $newitem
        case '*'
            printf "%ls: unexpected subcommand '%ls'\n" $cmd $hist_cmd
            return 2
    end
end
