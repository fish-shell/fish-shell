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
set -g fish_prompt_hg_status_unmerged '!'

set -g fish_prompt_hg_status_order added modified copied deleted untracked unmerged

function __fish_hg_prompt --description 'Write out the hg prompt'
	# If hg isn't installed, there's nothing we can do
	# Return 1 so the calling prompt can deal with it
	if not command -s hg >/dev/null
		return 1
	end

    set -l branch (hg branch ^/dev/null)
	# If there's no branch, there's no repository
    if test -z $branch
        return
    end

	# With "-q", hg bookmark will always output every bookmark
	# So our only option is to filter it ourselves
	set -l bookmark (hg bookmark | string match ' \\**' | cut -d" " -f3)
	# Unfortunately, hg bookmark doesn't exit non-zero when there's no bookmark
	if test -n "$bookmark"
		set branch "$branch/$bookmark"
	end

    echo -n '|'

    set -l repo_status (hg status | cut -c 1-2 | sort -u)

    # Show nice color for a clean repo
    if test -z "$repo_status"
        set_color $fish_color_hg_clean
        echo -n $branch'✓'
        set_color normal

    # Handle modified or dirty (unknown state)
    else
        set -l hg_statuses

        # Take actions for the statuses of the files in the repo
        for line in $repo_status

            # Add a character for each file status if we have one
            switch $line
                case 'A '               ; set hg_statuses $hg_statuses added
                case 'M ' ' M'          ; set hg_statuses $hg_statuses modified
                case 'C '               ; set hg_statuses $hg_statuses copied
                case 'D ' ' D'          ; set hg_statuses $hg_statuses deleted
                case '\? '              ; set hg_statuses $hg_statuses untracked
                case 'U*' '*U' 'DD' 'AA'; set hg_statuses $hg_statuses unmerged
            end
        end

        if string match -r '^[AMCD]' $repo_status
            set_color $fish_color_hg_modified
        else
            set_color $fish_color_hg_dirty
        end

        echo -n $branch'⚡'

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
