# Adapted from __terlar_git_prompt

function fish_hg_prompt --description 'Write out the hg prompt'
    set -q fish_color_hg_clean
    or set -l fish_color_hg_clean green
    set -q fish_color_hg_modified
    or set -l fish_color_hg_modified yellow
    set -q fish_color_hg_dirty
    or set -l fish_color_hg_dirty red

    set -q fish_color_hg_added
    or set -l fish_color_hg_added green
    set -q fish_color_hg_renamed
    or set -l fish_color_hg_renamed magenta
    set -q fish_color_hg_copied
    or set -l fish_color_hg_copied magenta
    set -q fish_color_hg_deleted
    or set -l fish_color_hg_deleted red
    set -q fish_color_hg_untracked
    or set -l fish_color_hg_untracked yellow
    set -q fish_color_hg_unmerged
    or set -l fish_color_hg_unmerged red

    set -q fish_prompt_hg_status_added
    or set -l fish_prompt_hg_status_added '✚'
    set -q fish_prompt_hg_status_modified
    or set -l fish_prompt_hg_status_modified '*'
    set -q fish_prompt_hg_status_copied
    or set -l fish_prompt_hg_status_copied '⇒'
    set -q fish_prompt_hg_status_deleted
    or set -l fish_prompt_hg_status_deleted '✖'
    set -q fish_prompt_hg_status_untracked
    or set -l fish_prompt_hg_status_untracked '?'
    set -q fish_prompt_hg_status_unmerged
    or set -l fish_prompt_hg_status_unmerged !

    set -q fish_prompt_hg_status_order
    or set -l fish_prompt_hg_status_order added modified copied deleted untracked unmerged

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

    # Strip control characters for display.
    set branch (string replace -ra '[[:cntrl:]]' '' -- $branch)

    if not set -q fish_prompt_hg_show_informative_status
        set_color --reset
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
        set_color --reset

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

        set_color --reset
    end

end
