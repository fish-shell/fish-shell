# name: Scales
# author: Leonard Hecker <leonard@hecker.io>

# Sources:
# - General theme setup: https://github.com/sorin-ionescu/prezto/blob/d275f316ffdd0bbd075afbff677c3e00791fba16/modules/prompt/functions/prompt_sorin_setup
# - Extraction of git info: https://github.com/sorin-ionescu/prezto/blob/d275f316ffdd0bbd075afbff677c3e00791fba16/modules/git/functions/git-info#L180-L441

function fish_prompt
    if test -n "$SSH_TTY"
        echo -n (set_color brred)"$USER"(set_color white)'@'(set_color yellow)(prompt_hostname)' '
    end

    echo -n (set_color blue)(prompt_pwd)' '

    set_color -o
    if fish_is_root_user
        echo -n (set_color red)'# '
    end
    echo -n (set_color red)'❯'(set_color yellow)'❯'(set_color green)'❯ '
    set_color normal
end

# And now define the right prompt so that it's brought along
function fish_right_prompt
    set -l cmd_status $status
    if test $cmd_status -ne 0
        echo -n (set_color red)"✘ $cmd_status"
    end

    if not command -sq git
        set_color normal
        return
    end

    # Get the git directory for later use.
    # Return if not inside a Git repository work tree.
    if not set -l git_dir (command git rev-parse --git-dir 2>/dev/null)
        set_color normal
        return
    end

    # Get the current action ("merge", "rebase", etc.)
    # and if there's one get the current commit hash too.
    set -l commit ''
    if set -l action (fish_print_git_action "$git_dir")
        set commit (command git rev-parse HEAD 2> /dev/null | string sub -l 7)
    end

    # Get either the branch name or a branch descriptor.
    set -l branch_detached 0
    if not set -l branch (command git symbolic-ref --short HEAD 2>/dev/null)
        set branch_detached 1
        set branch (command git describe --contains --all HEAD 2>/dev/null)
    end

    # Get the commit difference counts between local and remote.
    command git rev-list --count --left-right 'HEAD...@{upstream}' 2>/dev/null \
        | read -d \t -l status_ahead status_behind
    if test $status -ne 0
        set status_ahead 0
        set status_behind 0
    end

    # Get the stash status.
    # (git stash list) is very slow. => Avoid using it.
    set -l status_stashed 0
    if test -f "$git_dir/refs/stash"
        set status_stashed 1
    else if test -r "$git_dir/commondir"
        read -l commondir <"$git_dir/commondir"
        if test -f "$commondir/refs/stash"
            set status_stashed 1
        end
    end

    # git-status' porcelain v1 format starts with 2 letters on each line:
    #   The first letter (X) denotes the index state.
    #   The second letter (Y) denotes the working directory state.
    #
    # The following table presents the possible combinations:
    # * The underscore character denotes whitespace.
    # * The cell values stand for the following file states:
    #   a: added
    #   d: deleted
    #   m: modified
    #   r: renamed
    #   u: unmerged
    #   t: untracked
    # * Cells with more than one letter signify that both states
    #   are simultaneously the case. This is possible since the git index
    #   and working directory operate independently of each other.
    # * Cells which are empty are unhandled by this code.
    # * T (= type change) is undocumented.
    #   See Git v1.7.8.2 release notes for more information.
    #
    #   \ Y→
    #  X \
    #  ↓  | A  | C  | D  | M  | R  | T  | U  | X  | B  | ?  | _
    # ----+----+----+----+----+----+----+----+----+----+----+----
    #  A  | u  |    | ad | am | r  | am | u  |    |    |    | a
    #  C  |    |    | ad | am | r  | am | u  |    |    |    | a
    #  D  |    |    | u  | am | r  | am | u  |    |    |    | a
    #  M  |    |    | ad | am | r  | am | u  |    |    |    | a
    #  R  | r  | r  | rd | rm | r  | rm | ur | r  | r  | r  | r
    #  T  |    |    | ad | am | r  | am | u  |    |    |    | a
    #  U  | u  | u  | u  | um | ur | um | u  | u  | u  | u  | u
    #  X  |    |    |    | m  | r  | m  | u  |    |    |    |
    #  B  |    |    |    | m  | r  | m  | u  |    |    |    |
    #  ?  |    |    |    | m  | r  | m  | u  |    |    | t  |
    #  _  |    |    | d  | m  | r  | m  | u  |    |    |    |
    set -l porcelain_status (command git status --porcelain 2>/dev/null | string sub -l2)

    set -l status_added 0
    if string match -qr '[ACDMT][ MT]|[ACMT]D' $porcelain_status
        set status_added 1
    end
    set -l status_deleted 0
    if string match -qr '[ ACMRT]D' $porcelain_status
        set status_deleted 1
    end
    set -l status_modified 0
    if string match -qr '[MT]$' $porcelain_status
        set status_modified 1
    end
    set -l status_renamed 0
    if string match -qe R $porcelain_status
        set status_renamed 1
    end
    set -l status_unmerged 0
    if string match -qr 'AA|DD|U' $porcelain_status
        set status_unmerged 1
    end
    set -l status_untracked 0
    if string match -qe '\?\?' $porcelain_status
        set status_untracked 1
    end

    set_color -o

    if test -n "$branch"
        if test $branch_detached -ne 0
            set_color brmagenta
        else
            set_color green
        end
        echo -n " $branch"
    end
    if test -n "$commit"
        echo -n ' '(set_color yellow)"$commit"
    end
    if test -n "$action"
        set_color normal
        echo -n (set_color white)':'(set_color -o brred)"$action"
    end
    if test $status_ahead -ne 0
        echo -n ' '(set_color brmagenta)'⬆'
    end
    if test $status_behind -ne 0
        echo -n ' '(set_color brmagenta)'⬇'
    end
    if test $status_stashed -ne 0
        echo -n ' '(set_color cyan)'✭'
    end
    if test $status_added -ne 0
        echo -n ' '(set_color green)'✚'
    end
    if test $status_deleted -ne 0
        echo -n ' '(set_color red)'✖'
    end
    if test $status_modified -ne 0
        echo -n ' '(set_color blue)'✱'
    end
    if test $status_renamed -ne 0
        echo -n ' '(set_color magenta)'➜'
    end
    if test $status_unmerged -ne 0
        echo -n ' '(set_color yellow)'═'
    end
    if test $status_untracked -ne 0
        echo -n ' '(set_color white)'◼'
    end

    set_color normal
end
