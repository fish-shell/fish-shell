# Given the path to a .git directory this function prints a human-readable name
# for the git action in progress (e.g. "merge") or returns 1.
function fish_print_git_action --argument-names git_dir
    if test -z "$git_dir"
        if not command -sq git
            return 1
        end
        if not set git_dir (command git rev-parse --git-dir 2>/dev/null)
            return 1
        end
    end

    for action_dir in "$git_dir/rebase-apply" "$git_dir/rebase"
        if test -d "$action_dir"
            if test -f "$action_dir/rebasing"
                echo -n rebase
            else if test -f "$action_dir/applying"
                echo -n apply
            else
                echo -n rebase/apply
            end
            return 0
        end
    end

    for action_dir in "$git_dir/rebase-merge/interactive" "$git_dir/.dotest-merge/interactive"
        if test -f "$action_dir"
            echo -n rebase-interactive
            return 0
        end
    end

    for action_dir in "$git_dir/rebase-merge" "$git_dir/.dotest-merge"
        if test -d "$action_dir"
            echo -n rebase-merge
            return 0
        end
    end

    if test -f "$git_dir/MERGE_HEAD"
        echo -n merge
        return 0
    end

    if test -f "$git_dir/CHERRY_PICK_HEAD"
        if test -d "$git_dir/sequencer"
            echo -n cherry-pick-sequence
        else
            echo -n cherry-pick
        end
        return 0
    end

    if test -f "$git_dir/REVERT_HEAD"
        if test -d "$git_dir/sequencer"
            echo -n revert-sequence
        else
            echo -n revert
        end
        return 0
    end

    if test -f "$git_dir/BISECT_LOG"
        echo -n bisect
        return 0
    end

    return 1
end
