#
# Written by Mariusz Smykula <mariuszs at gmail.com>
#
# This is fish port of Informative git prompt for bash (https://github.com/magicmonty/bash-git-prompt)
#

set -g fish_color_git_clean green
set -g fish_color_git_branch magenta
set -g fish_color_git_remote green

set -g fish_color_git_staged yellow
set -g fish_color_git_conflicted red
set -g fish_color_git_changed blue
set -g fish_color_git_untracked $fish_color_normal

set -g fish_prompt_git_remote_ahead_of "↑"
set -g fish_prompt_git_remote_behind  "↓"

set -g fish_prompt_git_status_staged "●"
set -g fish_prompt_git_status_conflicted '✖'
set -g fish_prompt_git_status_changed '✚'
set -g fish_prompt_git_status_untracked "…"
set -g fish_prompt_git_status_clean "✔"

set -g fish_prompt_git_status_git_dir "*"

set -g fish_prompt_git_status_order staged conflicted changed untracked

function __informative_git_prompt --description 'Write out the git prompt'

    set -l is_inside_work_tree (git rev-parse --is-inside-work-tree ^/dev/null )

    if test -z $is_inside_work_tree
        return
    end

    if test "true" = $is_inside_work_tree
        set git_status_info (___fish_git_print_status_info)
    else
        set git_status_info $fish_prompt_git_status_git_dir
    end

    printf "(%s|%s)" (___fish_git_print_branch_info) $git_status_info

end



function ___fish_git_print_branch_info

    set -l color_branch (set_color -o $fish_color_git_branch)
    set -l color_normal (set_color $fish_color_normal)

    set -l branch (git symbolic-ref -q HEAD | cut -c 12-)
    set -l remote_info

    if test -z $branch
        set -l hash (git rev-parse --short HEAD | cut -c 2-)
        set branch ":"$hash
    else
        set remote_info (___fish_git_print_remote_info $branch)
    end

    echo "$color_branch$branch$color_normal$remote_info"

end

function ___fish_git_print_status_info

    set -l color_normal (set_color $fish_color_normal)
    set -l color_git_clean (set_color -o $fish_color_git_clean)

    set -l changedFiles (git diff --name-status | cut -c 1-2)
    set -l stagedFiles (git diff --staged --name-status | cut -c 1-2)

    set -l changed (math (count $changedFiles) - (count (echo $changedFiles | grep "U")))
    set -l conflicted (count (echo $stagedFiles | grep "U"))
    set -l staged (math (count $stagedFiles) - $conflicted)
    set -l untracked (count (git ls-files --others --exclude-standard))

    if [ (math $changed + $conflicted + $staged + $untracked) = 0 ]
        set git_status $color_git_clean$fish_prompt_git_status_clean$color_normal
    else
        for i in $fish_prompt_git_status_order
            if [ $$i != "0" ]
                set -l color_name fish_color_git_$i
                set -l status_name fish_prompt_git_status_$i
                set -l color (set_color $$color_name)
                set -l info $$status_name$$i
                set git_status "$git_status$color$info"
            end
        end
    end

    echo $git_status$color_normal

end

function ___fish_git_print_remote_info

    set color_remote (set_color -o $fish_color_git_remote)
    set color_normal (set_color $fish_color_normal)

    set -l branch $argv[1]
    set -l remote (____fish_git_remote_info $branch)
    set -l ahead $remote[1]
    set -l behind $remote[2]
    set -l remote_info


    if [ $ahead != "0" ]
        set remote_info $color_remote$fish_prompt_git_remote_ahead_of$color_normal$ahead
    end

    if [ $behind != "0" ]
        set remote_info $remote_info$color_remote$fish_prompt_git_remote_behind$color_normal$behind
    end

    if test -n "$remote_info"
        echo " $remote_info"
    end

end

function ____fish_git_remote_info

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

    set -l rev_git (eval "git rev-list --left-right $remote_ref...HEAD" ^/dev/null)
    if test $status != "0"
        set rev_git (eval "git rev-list --left-right $merge_name...HEAD" ^/dev/null)
    end

    for i in $rev_git
        if echo $i | grep '>' >/dev/null
           set isAhead $isAhead ">"
        end
    end

    set -l remote_diff (count $rev_git)
    set -l ahead (count $isAhead)
    set -l behind (math $remote_diff - $ahead)

    echo $ahead
    echo $behind

end

