#
# Written by Mariusz Smykula <mariuszs at gmail.com>
#
# This is fish port of Informative git prompt for bash (https://github.com/magicmonty/bash-git-prompt)
#

set -gx fish_color_git_clean green
set -gx fish_color_git_branch magenta
set -gx fish_color_git_remote green

set -gx fish_color_git_staged yellow
set -gx fish_color_git_conflicts red
set -gx fish_color_git_changed blue
set -gx fish_color_git_untracked $fish_color_normal

set -gx fish_prompt_git_remote_ahead_of "↑"
set -gx fish_prompt_git_remote_behind  "↓"

set -gx fish_prompt_git_status_staged "●"
set -gx fish_prompt_git_status_conflicts '✖'
set -gx fish_prompt_git_status_changed '✚'
set -gx fish_prompt_git_status_untracked "…"
set -gx fish_prompt_git_status_clean "✔"

set -gx fish_prompt_git_status_order staged conflicts changed untracked

function __informative_git_prompt --description 'Write out the git prompt'

    set -l branch (git rev-parse --abbrev-ref HEAD ^/dev/null)
    if test -z $branch
        return
    end

    set -l changedFiles (git diff --name-status | cut -c 1-2)
    set -l stagedFiles (git diff --staged --name-status | cut -c 1-2)

    set -l changed (math (count $changedFiles) - (count (echo $changedFiles | grep "U")))
    set -l conflicts (count (echo $stagedFiles | grep "U"))
    set -l staged (math (count $stagedFiles) - $conflicts)
    set -l untracked (count (git ls-files --others --exclude-standard))

    set -l branch (git symbolic-ref -q HEAD | cut -c 12-)

    echo -n " ("
    set_color -o $fish_color_git_branch

    if test -z $branch
        set hash (git rev-parse --short HEAD | cut -c 2-)
        echo -n ":"$hash
    else
        echo -n $branch
        ___print_remote_info $branch
    end

    set_color $fish_color_normal
    echo -n '|'

    if [ (math $changed + $conflicts + $staged + $untracked) = 0 ]
        set_color -o $fish_color_git_clean
        echo -n $fish_prompt_git_status_clean
        set_color $fish_color_normal
    end

    for i in $fish_prompt_git_status_order
        if [ $$i != "0" ]
            set -l color_name fish_color_git_$i
            set -l status_name fish_prompt_git_status_$i

            set_color $$color_name
            echo -n $$status_name$$i
        end
    end

    set_color $fish_color_normal

    echo -n ")"

end

function ___print_remote_info

 set -l branch $argv[1]
 set -l remote_name  (git config branch.$branch.remote)

    if test -n "$remote_name"
        set merge_name (git config branch.$branch.merge)
        set merge_name_short (echo $merge_name | cut -c 12-)
    else
        set remote_name "origin"
        set merge_name "refs/heads/$branch"
        set merge_name_short $branch
    end

    if [ $remote_name = '.' ]  # local
        set remote_ref $merge_name
    else
        set remote_ref "refs/remotes/$remote_name/$merge_name_short"
    end

    set rev_git (eval "git rev-list --left-right $remote_ref...HEAD" ^/dev/null)
    if test $status != "0"
        set rev_git (git rev-list --left-right $merge_name...HEAD)
    end

    for i in $rev_git
        if echo $i | grep '>' >/dev/null
           set isAhead $isAhead ">"
        end
    end

    set -l remote_diff (count $rev_git)
    set -l ahead (count $isAhead)
    set -l behind (math $remote_diff - $ahead)

    if [ $remote_diff != "0" ]
        echo -n " "
    end

    if [ $ahead != "0" ]
        set_color -o $fish_color_git_remote
        echo -n $fish_prompt_git_remote_ahead_of
        set_color $fish_color_normal
        echo -n $ahead
    end

    if [ $behind != "0" ]
        set_color -o $fish_color_git_remote
        echo -n $fish_prompt_git_remote_behind
        set_color $fish_color_normal
        echo -n $behind
    end

end
