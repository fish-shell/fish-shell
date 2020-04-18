# Adapted from __terlar_git_prompt

set -g fish_color_hg_clean green
set -g fish_color_hg_modified yellow
set -g fish_color_hg_dirty red

set -g fish_color_hg_added green
set -g fish_color_hg_renamed magenta
set -g fish_color_hg_copied magenta
set -g fish_color_hg_deleted red
set -g fish_color_hg_untracked yellow
set -g fish_color_hg_unmerged red

set -g fish_prompt_hg_status_added '✚'
set -g fish_prompt_hg_status_modified '*'
set -g fish_prompt_hg_status_copied '⇒'
set -g fish_prompt_hg_status_deleted '✖'
set -g fish_prompt_hg_status_untracked '?'
set -g fish_prompt_hg_status_unmerged !

set -g fish_prompt_hg_status_order added modified copied deleted untracked unmerged

function fish_hg_prompt --description 'Write out the hg prompt'
    # If hg isn't installed, there's nothing we can do
    # Return 1 so the calling prompt can deal with it
    if not command -sq hg
        return 1
    end

    set -l root (fish_print_hg_root)
    or return 1

    # Read branch and bookmark
    set -l branch (cat $root/branch 2>/dev/null; or echo default)
    if set -l bookmark (cat $root/bookmarks.current 2>/dev/null)
        set branch "$branch|$bookmark"
    end

    if not set -q fish_prompt_hg_show_informative_status
        set_color normal
        echo -n " ($branch)"
        return
    end

    echo -n '|'

    # Disabling color and pager is always a good idea.
    set -l repo_status (hg status | string sub -l 2 | sort -u)

    # Show nice color for a clean repo
    if test -z "$repo_status"
        set_color $fish_color_hg_clean
        echo -n "($branch)"'✓'
        set_color normal

        # Handle modified or dirty (unknown state)
    else
        set -l hg_statuses

        # Take actions for the statuses of the files in the repo
        for line in $repo_status

            # Add a character for each file status if we have one
            # HACK: To allow this to work both with and without '?' globs
            set -l dq '?'
            switch $line
                case 'A '
                    set -a hg_statuses added
                case 'M ' ' M'
                    set -a hg_statuses modified
                case 'C '
                    set -a hg_statuses copied
                case 'D ' ' D'
                    set -a hg_statuses deleted
                case "$dq "
                    set -a hg_statuses untracked
                case 'U*' '*U' DD AA
                    set -a hg_statuses unmerged
            end
        end

        if string match -qr '^[AMCD]' $repo_status
            set_color $fish_color_hg_modified
        else
            set_color $fish_color_hg_dirty
        end

        echo -n "($branch)"'⚡'

        # Sort status symbols
        for i in $fish_prompt_hg_status_order
            if contains -- $i $hg_statuses
                set -l color_name fish_color_hg_$i
                set -l status_name fish_prompt_hg_status_$i

                set_color $$color_name
                echo -n $$status_name
            end
        end

        set_color normal
    end

end
