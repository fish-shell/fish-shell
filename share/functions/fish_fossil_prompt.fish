function fish_fossil_prompt --description 'Write out the fossil prompt'
    # Bail if fossil is not available
    if not command -sq fossil
        return 1
    end

    # Read branch and bookmark (bail if not checkout)
    set -l branch (fossil branch current 2>/dev/null)
    or return 127

    set -q fish_color_fossil_clean
    or set -g fish_color_fossil_clean green
    set -q fish_color_fossil_modified
    or set -g fish_color_fossil_modified yellow
    set -q fish_color_fossil_dirty
    or set -g fish_color_fossil_dirty red

    set -q fish_color_fossil_added
    or set -g fish_color_fossil_added green
    set -q fish_color_fossil_renamed
    or set -g fish_color_fossil_renamed magenta
    set -q fish_color_fossil_missing
    or set -g fish_color_fossil_missing red
    set -q fish_color_fossil_deleted
    or set -g fish_color_fossil_deleted red
    set -q fish_color_fossil_untracked
    or set -g fish_color_fossil_untracked yellow
    set -q fish_color_fossil_conflict
    or set -g fish_color_fossil_conflict red

    set -q fish_prompt_fossil_status_added
    or set -g fish_prompt_fossil_status_added '✚'
    set -q fish_prompt_fossil_status_modified
    or set -g fish_prompt_fossil_status_modified '*'
    set -q fish_prompt_fossil_status_renamed
    or set -g fish_prompt_fossil_status_renamed '⇒'
    set -q fish_prompt_fossil_status_deleted
    or set -g fish_prompt_fossil_status_deleted -
    set -q fish_prompt_fossil_status_missing
    or set -g fish_prompt_fossil_status_missing '✖'
    set -q fish_prompt_fossil_status_untracked
    or set -g fish_prompt_fossil_status_untracked '?'
    set -q fish_prompt_fossil_status_conflict
    or set -g fish_prompt_fossil_status_conflict '×'

    set -q fish_prompt_fossil_status_order
    or set -g fish_prompt_fossil_status_order added modified renamed deleted missing untracked conflict

    echo -n ' ('
    set_color magenta
    echo -n "$branch"
    set_color normal
    echo -n '|'
    #set -l repo_status (fossil changes --differ 2>/dev/null | string match -rv '\w:|^\s' | string split " " -f1 | sort -u)
    set -l repo_status (fossil changes --differ 2>/dev/null | string match -rv '\w:|^\s' | string split " " -f1 | path sort -u)

    # Show nice color for a clean repo
    if test -z "$repo_status"
        set_color $fish_color_fossil_clean
        echo -n '✔'

        # Handle modified or dirty (unknown state)
    else
        set -l fossil_statuses

        # Take actions for the statuses of the files in the repo
        for line in $repo_status

            # Add a character for each file status if we have one
            switch $line
                case ADDED
                    set -a fossil_statuses added
                case EDITED
                    set -a fossil_statuses modified
                case EXTRA
                    set -a fossil_statuses untracked
                case DELETED
                    set -a fossil_statuses deleted
                case MISSING
                    set -a fossil_statuses missing
                case RENAMED
                    set -a fossil_statuses renamed
                case CONFLICT
                    set -a fossil_statuses conflict
            end
        end

        if string match -qr '^(ADDED|EDITED|DELETED)' $repo_status
            set_color $fish_color_fossil_modified
        else
            set_color --bold $fish_color_fossil_dirty
        end

        echo -n '⚡'
        set_color normal

        # Sort status symbols
        for i in $fish_prompt_fossil_status_order
            if contains -- $i $fossil_statuses
                set -l color_name fish_color_fossil_$i
                set -l status_name fish_prompt_fossil_status_$i

                set_color $$color_name
                echo -n $$status_name
            end
        end
    end

    set_color normal
    echo -n ')'
end
