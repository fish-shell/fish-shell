# based off of the git-prompt script that ships with git
#
# Written by Kevin Ballard <kevin@sb.org>
# Updated by Brian Gernhardt <brian@gernhardtsoftware.com>
#
# This is heavily based off of the git-prompt.bash script that ships with
# git, which is Copyright (C) 2006,2007 Shawn O. Pearce <spearce@spearce.org>.
# The act of porting the code, along with any new code, are Copyright (C) 2012
# Kevin Ballard <kevin@sb.org>.
#
# By virtue of being based on the git-prompt.bash script, this script is
# distributed under the GNU General Public License, version 2.0.
#
# This script includes a function fish_git_prompt which takes a format string,
# exactly how the bash script works. This can be used in your fish_prompt
# function.
#
# The behavior of fish_git_prompt is very heavily based off of the bash
# script's __git_ps1 function. As such, usage and customization is very
# similar, although some extra features are provided in this script.
# Due to differences between bash and fish, the PROMPT_COMMAND style where
# passing two or three arguments causes the function to set PS1 is not
# supported.  More information on the additional features is found after the
# bash-compatable documentation.
#
# The argument to fish_git_prompt will be displayed only if you are currently
# in a git repository. The %s token will be the name of the branch.
#
# In addition, if you set __fish_git_prompt_showdirtystate to a nonempty value,
# unstaged (*) and staged (+) changes will be shown next to the branch name.
# You can configure this per-repository with the bash.showDirtyState variable,
# which defaults to true once __fish_git_prompt_showdirtystate is enabled. The
# choice to leave the variable as 'bash' instead of renaming to 'fish' is done
# to preserve compatibility with existing configured repositories.
#
# You can also see if currently something is stashed, by setting
# __fish_git_prompt_showstashstate to a nonempty value. If something is
# stashed, then a '$' will be shown next to the branch name.
#
# If you would like to see if there are untracked files, then you can set
# __fish_git_prompt_showuntrackedfiles to a nonempty value. If there are
# untracked files, then a '%' will be shown next to the branch name. Once you
# have set __fish_git_prompt_showuntrackedfiles, you can override it on a
# per-repository basis by setting the bash.showUntrackedFiles config variable.
# As before, this variable remains named 'bash' to preserve compatibility.
#
# If you would like to see the difference between HEAD and its upstream, set
# __fish_git_prompt_showupstream to 'auto'. A "<" indicates you are behind, ">"
# indicates you are ahead, "<>" indicates you have diverged and "=" indicates
# that there is no difference. You can further control behavior by setting
# __fish_git_prompt_showupstream to a space-separated list of values:
#
#     verbose        show number of commits ahead/behind (+/-) upstream
#     name           if verbose, then also show the upstream abbrev name
#     informative    similar to verbose, but shows nothing when equal (fish only)
#     git            always compare HEAD to @{upstream}
#     svn            always compare HEAD to your SVN upstream
#     none           disables (fish only, useful with show_informative_status)
#
# By default, fish_git_prompt will compare HEAD to your SVN upstream if it
# can find one, or @{upstream} otherwise. Once you have set
# __fish_git_prompt_showupstream, you can override it on a per-repository basis
# by setting the bash.showUpstream config variable. As before, this variable
# remains named 'bash' to preserve compatibility.
#
# If you would like to see more information about the identity of commits
# checked out as a detached HEAD, set __fish_git_prompt_describe_style to
# one of the following values:
#
#     contains      relative to newer annotated tag (v1.6.3.2~35)
#     branch        relative to newer tag or branch (master~4)
#     describe      relative to older annotated tag (v1.6.3.1-13-gdd42c2f)
#     default       exactly matching tag
#
# If you would like a colored hint about the current dirty state, set
# __fish_git_prompt_showcolorhints.  The default colors are
# based on the colored output of "git status -sb"


# fish_git_prompt includes some additional features on top of the
# above-documented bash-compatible features:
#
#
# An "informative git prompt" mode similar to the scripts for bash and zsh
# can be activated by setting __fish_git_prompt_show_informative_status
# This works more like the "informative git prompt" scripts for bash and zsh,
# giving prompts like (master↑1↓2|●3✖4✚5…6) where master is the current branch,
# you have 1 commit your upstream doesn't and it has 2 you don't, and you have
# 3 staged, 4 unmerged, 5 dirty, and 6 untracked files.  If you have no
# changes, it displays (master|✔).
#
# Setting __fish_git_prompt_show_informative_status changes several defaults.
# The default mode for __fish_git_prompt_showupstream changes to informative
# and the following characters have their defaults changed.  (The characters
# and colors can still be customized as described below.)
#
#     upstream_prefix ()
#     upstream_ahead  (↑)
#     upstream_behind (↓)
#     stateseparator  (|)
#     dirtystate      (✚)
#     invalidstate    (✖)
#     stagedstate     (●)
#     untrackedfiles  (…)
#     stashstate      (⚑)
#     cleanstate      (✔)
#
#
# The color for each component of the prompt can specified using
# __fish_git_prompt_color_<name>, where <name> is one of the following and the
# values are specified as arguments to `set_color`.  The variable
# __fish_git_prompt_color is used for any component that does not have an
# individual color set.
#
#     prefix     Anything before %s in the format string
#     suffix     Anything after  %s in the format string
#     bare       Marker for a bare repository
#     merging    Current operation (|MERGING, |REBASE, etc.)
#     branch     Branch name
#     flags      Optional flags (see below)
#     upstream   Upstream name and flags (with showupstream)
#
#
# The following optional flags have both colors, as above, and custom
# characters via __fish_git_prompt_char_<name>.  The default character is
# shown in parenthesis.  The default color for these flags can be also be set
# via the __fish_git_prompt_color_flags variable.
#
#   __fish_git_prompt_showdirtystate
#     dirtystate          unstaged changes (*)
#     stagedstate         staged changes   (+)
#     invalidstate        HEAD invalid     (#, colored as stagedstate)
#
#   __fish_git_prompt_showstashstate
#     stashstate          stashed changes  ($)
#
#   __fish_git_prompt_showuntrackedfiles
#     untrackedfiles      untracked files  (%)
#
#   __fish_git_prompt_showupstream  (all colored as upstream)
#     upstream_equal      Branch matches upstream              (=)
#     upstream_behind     Upstream has more commits            (<)
#     upstream_ahead      Branch has more commits              (>)
#     upstream_diverged   Upstream and branch have new commits (<>)
#
#   __fish_git_prompt_show_informative_status
#     (see also the flags for showdirtystate and showuntrackedfiles, above)
#     cleanstate          Working directory has no changes (✔)
#
#
# The separator between the branch name and flags can also be customized via
# __fish_git_prompt_char_stateseparator.  It can only be colored by
# __fish_git_prompt_color.  It normally defaults to a space ( ) and defaults
# to a vertical bar (|) when __fish_git_prompt_show_informative_status is set.
#
# The separator before the upstream information can be customized via
# __fish_git_prompt_char_upstream_prefix.  It is colored like the rest of
# the upstream information.  It normally defaults to nothing () and defaults
# to a space ( ) when __fish_git_prompt_showupstream contains verbose.
#
#
# Turning on __fish_git_prompt_showcolorhints changes the colors as follows to
# more closely match the behavior in bash.  Note that setting any of these
# colors manually will override these defaults.
#
#     branch            Defaults to green
#     branch_detached   New color, when head is detached, default red
#     dirtystate        Defaults to red
#     stagedstate       Defaults to green
#     flags             Defaults to --bold blue
#
#
# The branch name could be shorten via
# __fish_git_prompt_shorten_branch_len. Define the branch max len.
# __fish_git_prompt_shorten_branch_char_suffix. Customize suffixed char of shorten branch. Defaults to (…).

function __fish_git_prompt_show_upstream --description "Helper function for fish_git_prompt"
    set -l show_upstream $__fish_git_prompt_showupstream
    set -l svn_prefix # For better SVN upstream information
    set -l informative

    set -l svn_url_pattern
    set -l count
    set -l upstream git
    set -l verbose
    set -l name

    # Default to informative if __fish_git_prompt_show_informative_status is set
    if set -q __fish_git_prompt_show_informative_status
        set informative 1
    end

    set -l svn_remote
    # get some config options from git-config
    command git config -z --get-regexp '^(svn-remote\..*\.url|bash\.showupstream)$' 2>/dev/null | while read -lz key value
        switch $key
            case bash.showupstream
                set show_upstream $value
                test -n "$show_upstream"
                or return
            case svn-remote.'*'.url
                set svn_remote $svn_remote $value
                # Avoid adding \| to the beginning to avoid needing #?? later
                if test -n "$svn_url_pattern"
                    set svn_url_pattern $svn_url_pattern"|$value"
                else
                    set svn_url_pattern $value
                end
                set upstream svn+git # default upstream is SVN if available, else git

                # Save the config key (without .url) for later use
                set -l remote_prefix (string replace -r '\.url$' '' -- $key)
                set svn_prefix $svn_prefix $remote_prefix
        end
    end

    # parse configuration variables
    # and clear informative default when needed
    for option in $show_upstream
        switch $option
            case git svn
                set upstream $option
                set -e informative
            case verbose
                set verbose 1
                set -e informative
            case informative
                set informative 1
            case name
                set name 1
            case none
                return
        end
    end

    # Find our upstream
    switch $upstream
        case git
            set upstream '@{upstream}'
        case svn\*
            # get the upstream from the 'git-svn-id: …' in a commit message
            # (git-svn uses essentially the same procedure internally)
            set -l svn_upstream (git log --first-parent -1 --grep="^git-svn-id: \($svn_url_pattern\)" 2>/dev/null)
            if test (count $svn_upstream) -ne 0
                echo $svn_upstream[-1] | read -l __ svn_upstream __
                set svn_upstream (string replace -r '@.*' '' -- $svn_upstream)
                set -l cur_prefix
                for i in (seq (count $svn_remote))
                    set -l remote $svn_remote[$i]
                    set -l mod_upstream (string replace "$remote" "" -- $svn_upstream)
                    if test "$svn_upstream" != "$mod_upstream"
                        # we found a valid remote
                        set svn_upstream $mod_upstream
                        set cur_prefix $svn_prefix[$i]
                        break
                    end
                end

                if test -z "$svn_upstream"
                    # default branch name for checkouts with no layout:
                    if test -n "$GIT_SVN_ID"
                        set upstream $GIT_SVN_ID
                    else
                        set upstream git-svn
                    end
                else
                    set upstream (string replace '/branches' '' -- $svn_upstream | string replace -a '/' '')

                    # Use fetch config to fix upstream
                    set -l fetch_val (command git config "$cur_prefix".fetch)
                    if test -n "$fetch_val"
                        string split -m1 : -- "$fetch_val" | read -l trunk pattern
                        set upstream (string replace -r -- "/$trunk\$" '' $pattern) /$upstream
                    end
                end
            else if test $upstream = svn+git
                set upstream '@{upstream}'
            end
    end

    # Find how many commits we are ahead/behind our upstream
    set count (command git rev-list --count --left-right $upstream...HEAD 2>/dev/null | string replace \t " ")

    # calculate the result
    if test -n "$verbose"
        # Verbose has a space by default
        set -l prefix "$___fish_git_prompt_char_upstream_prefix"
        # Using two underscore version to check if user explicitly set to nothing
        if not set -q __fish_git_prompt_char_upstream_prefix
            set prefix " "
        end

        echo $count | read -l behind ahead
        switch "$count"
            case '' # no upstream
            case "0 0" # equal to upstream
                echo "$prefix$___fish_git_prompt_char_upstream_equal"
            case "0 *" # ahead of upstream
                echo "$prefix$___fish_git_prompt_char_upstream_ahead$ahead"
            case "* 0" # behind upstream
                echo "$prefix$___fish_git_prompt_char_upstream_behind$behind"
            case '*' # diverged from upstream
                echo "$prefix$___fish_git_prompt_char_upstream_diverged$ahead-$behind"
        end
        if test -n "$count" -a -n "$name"
            echo " "(command git rev-parse --abbrev-ref "$upstream" 2>/dev/null)
        end
    else if test -n "$informative"
        echo $count | read -l behind ahead
        switch "$count"
            case '' # no upstream
            case "0 0" # equal to upstream
            case "0 *" # ahead of upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead"
            case "* 0" # behind upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind$behind"
            case '*' # diverged from upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead$___fish_git_prompt_char_upstream_behind$behind"
        end
    else
        switch "$count"
            case '' # no upstream
            case "0 0" # equal to upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_equal"
            case "0 *" # ahead of upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead"
            case "* 0" # behind upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind"
            case '*' # diverged from upstream
                echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_diverged"
        end
    end

    # For the return status
    test "$count" = "0 0"
end

function fish_git_prompt --description "Prompt function for Git"
    # If git isn't installed, there's nothing we can do
    # Return 1 so the calling prompt can deal with it
    if not command -sq git
        return 1
    end
    set -l repo_info (command git rev-parse --git-dir --is-inside-git-dir --is-bare-repository --is-inside-work-tree HEAD 2>/dev/null)
    test -n "$repo_info"
    or return

    set -l git_dir $repo_info[1]
    set -l inside_gitdir $repo_info[2]
    set -l bare_repo $repo_info[3]
    set -l inside_worktree $repo_info[4]
    set -q repo_info[5]
    and set -l sha $repo_info[5]

    set -l rbc (__fish_git_prompt_operation_branch_bare $repo_info)
    set -l r $rbc[1] # current operation
    set -l b $rbc[2] # current branch
    set -l detached $rbc[3]
    set -l w #dirty working directory
    set -l i #staged changes
    set -l s #stashes
    set -l u #untracked
    set -l c $rbc[4] # bare repository
    set -l p #upstream
    set -l informative_status

    if not set -q ___fish_git_prompt_init
        # This takes a while, so it only needs to be done once,
        # and then whenever the configuration changes.
        __fish_git_prompt_validate_chars
        __fish_git_prompt_validate_colors
        set -g ___fish_git_prompt_init
    end

    set -l space "$___fish_git_prompt_color$___fish_git_prompt_char_stateseparator$___fish_git_prompt_color_done"

    # Use our variables as defaults, but allow overrides via the local git config.
    # That means if neither is set, this stays empty.
    #
    # So "!= true" or "!= false" are useful tests if you want to do something by default.
    set -l informative (command git config --bool bash.showInformativeStatus)

    set -l dirty (command git config --bool bash.showDirtyState)
    if not set -q dirty[1]
        set -q __fish_git_prompt_showdirtystate
        and set dirty true
    end

    set -l untracked (command git config --bool bash.showUntrackedFiles)
    if not set -q untracked[1]
        set -q __fish_git_prompt_showuntrackedfiles
        and set untracked true
    end

    if test true = $inside_worktree
        # Use informative status if it has been enabled locally, or it has been
        # enabled globally (via the fish variable) and dirty or untracked are not false.
        #
        # This is to allow overrides for the repository.
        if test "$informative" = true
            or begin
                set -q __fish_git_prompt_show_informative_status
                and test "$dirty" != false
                and test "$untracked" != false
            end
            set informative_status (__fish_git_prompt_informative_status $git_dir)
            if test -n "$informative_status"
                set informative_status "$space$informative_status"
            end
        else
            # This has to be set explicitly.
            if test "$dirty" = true
                set w (__fish_git_prompt_dirty)
                if test -n "$sha"
                    set i (__fish_git_prompt_staged)
                else
                    set i $___fish_git_prompt_char_invalidstate
                end
            end

            if set -q __fish_git_prompt_showstashstate
                and test -r $git_dir/logs/refs/stash
                set s $___fish_git_prompt_char_stashstate
            end

            if test "$untracked" = true
                set u (__fish_git_prompt_untracked)
            end
        end

        if set -q __fish_git_prompt_showupstream
            or set -q __fish_git_prompt_show_informative_status
            set p (__fish_git_prompt_show_upstream)
        end
    end

    set -l branch_color $___fish_git_prompt_color_branch
    set -l branch_done $___fish_git_prompt_color_branch_done
    if set -q __fish_git_prompt_showcolorhints
        if test $detached = yes
            set branch_color $___fish_git_prompt_color_branch_detached
            set branch_done $___fish_git_prompt_color_branch_detached_done
        end
    end

    if test -n "$w"
        set w "$___fish_git_prompt_color_dirtystate$w$___fish_git_prompt_color_dirtystate_done"
    end
    if test -n "$i"
        set i "$___fish_git_prompt_color_stagedstate$i$___fish_git_prompt_color_stagedstate_done"
    end
    if test -n "$s"
        set s "$___fish_git_prompt_color_stashstate$s$___fish_git_prompt_color_stashstate_done"
    end
    if test -n "$u"
        set u "$___fish_git_prompt_color_untrackedfiles$u$___fish_git_prompt_color_untrackedfiles_done"
    end

    set b (string replace refs/heads/ '' -- $b)
    set -q __fish_git_prompt_shorten_branch_char_suffix
    or set -l __fish_git_prompt_shorten_branch_char_suffix "…"
    if string match -qr '^\d+$' "$__fish_git_prompt_shorten_branch_len"; and test (string length "$b") -gt $__fish_git_prompt_shorten_branch_len
        set b (string sub -l "$__fish_git_prompt_shorten_branch_len" "$b")"$__fish_git_prompt_shorten_branch_char_suffix"
    end
    if test -n "$b"
        set b "$branch_color$b$branch_done"
    end

    if test -n "$c"
        set c "$___fish_git_prompt_color_bare$c$___fish_git_prompt_color_bare_done"
    end
    if test -n "$r"
        set r "$___fish_git_prompt_color_merging$r$___fish_git_prompt_color_merging_done"
    end
    if test -n "$p"
        set p "$___fish_git_prompt_color_upstream$p$___fish_git_prompt_color_upstream_done"
    end

    # Formatting
    set -l f "$w$i$s$u"
    if test -n "$f"
        set f "$space$f"
    end
    set -l format $argv[1]
    if test -z "$format"
        set format " (%s)"
    end

    printf "%s$format%s" "$___fish_git_prompt_color_prefix" "$___fish_git_prompt_color_prefix_done$c$b$f$r$p$informative_status$___fish_git_prompt_color_suffix" "$___fish_git_prompt_color_suffix_done"
end

### helper functions

function __fish_git_prompt_staged --description "fish_git_prompt helper, tells whether or not the current branch has staged files"
    # The "diff" functions all return > 0 if there _is_ a diff,
    # but we want to return 0 if there are staged changes.
    # So we invert the status.
    not command git diff-index --cached --quiet HEAD -- 2>/dev/null
    and echo $___fish_git_prompt_char_stagedstate
end

function __fish_git_prompt_untracked --description "fish_git_prompt helper, tells whether or not the current repository has untracked files"
    command git ls-files --others --exclude-standard --directory --no-empty-directory --error-unmatch -- :/ >/dev/null 2>&1
    and echo $___fish_git_prompt_char_untrackedfiles
end

function __fish_git_prompt_dirty --description "fish_git_prompt helper, tells whether or not the current branch has tracked, modified files"
    # Like staged, invert the status because we want 0 to mean there are dirty files.
    not command git diff --no-ext-diff --quiet --exit-code 2>/dev/null
    and echo $___fish_git_prompt_char_dirtystate
end

set -g ___fish_git_prompt_status_order stagedstate invalidstate dirtystate untrackedfiles stashstate

function __fish_git_prompt_informative_status

    set -l changedFiles (command git diff --name-status 2>/dev/null | string match -r \\w)
    set -l stagedFiles (command git diff --staged --name-status | string match -r \\w)

    set -l x (count $changedFiles)
    set -l y (count (string match -r "U" -- $changedFiles))
    set -l dirtystate (math $x - $y)
    set -l x (count $stagedFiles)
    set -l invalidstate (count (string match -r "U" -- $stagedFiles))
    set -l stagedstate (math $x - $invalidstate)
    set -l untrackedfiles (command git ls-files --others --exclude-standard :/ | count)
    set -l stashstate 0
    set -l stashfile "$argv[1]/logs/refs/stash"
    if set -q __fish_git_prompt_showstashstate; and test -e "$stashfile"
        set stashstate (count < $stashfile)
    end

    set -l info

    # If `math` fails for some reason, assume the state is clean - it's the simpler path
    set -l state (math $dirtystate + $invalidstate + $stagedstate + $untrackedfiles + $stashstate 2>/dev/null)
    if test -z "$state"
        or test "$state" = 0
        if test -n "$___fish_git_prompt_char_cleanstate"
            set info $___fish_git_prompt_color_cleanstate$___fish_git_prompt_char_cleanstate$___fish_git_prompt_color_cleanstate_done
        end
    else
        for i in $___fish_git_prompt_status_order
            if [ $$i != 0 ]
                set -l color_var ___fish_git_prompt_color_$i
                set -l color_done_var ___fish_git_prompt_color_{$i}_done
                set -l symbol_var ___fish_git_prompt_char_$i

                set -l color $$color_var
                set -l color_done $$color_done_var
                set -l symbol $$symbol_var

                set -l count

                if not set -q __fish_git_prompt_hide_$i
                    set count $$i
                end

                set info "$info$color$symbol$count$color_done"
            end
        end
    end

    echo $info

end

# Keeping these together avoids many duplicated checks
function __fish_git_prompt_operation_branch_bare --description "fish_git_prompt helper, returns the current Git operation and branch"
    # This function is passed the full repo_info array
    set -l git_dir $argv[1]
    set -l inside_gitdir $argv[2]
    set -l bare_repo $argv[3]
    set -q argv[5]
    and set -l sha $argv[5]

    set -l branch
    set -l operation
    set -l detached no
    set -l bare
    set -l step
    set -l total

    if test -d $git_dir/rebase-merge
        set branch (cat $git_dir/rebase-merge/head-name 2>/dev/null)
        set step (cat $git_dir/rebase-merge/msgnum 2>/dev/null)
        set total (cat $git_dir/rebase-merge/end 2>/dev/null)
        if test -f $git_dir/rebase-merge/interactive
            set operation "|REBASE-i"
        else
            set operation "|REBASE-m"
        end
    else
        if test -d $git_dir/rebase-apply
            set step (cat $git_dir/rebase-apply/next 2>/dev/null)
            set total (cat $git_dir/rebase-apply/last 2>/dev/null)
            if test -f $git_dir/rebase-apply/rebasing
                set branch (cat $git_dir/rebase-apply/head-name 2>/dev/null)
                set operation "|REBASE"
            else if test -f $git_dir/rebase-apply/applying
                set operation "|AM"
            else
                set operation "|AM/REBASE"
            end
        else if test -f $git_dir/MERGE_HEAD
            set operation "|MERGING"
        else if test -f $git_dir/CHERRY_PICK_HEAD
            set operation "|CHERRY-PICKING"
        else if test -f $git_dir/REVERT_HEAD
            set operation "|REVERTING"
        else if test -f $git_dir/BISECT_LOG
            set operation "|BISECTING"
        end
    end

    if test -n "$step" -a -n "$total"
        set operation "$operation $step/$total"
    end

    if test -z "$branch"
        if not set branch (command git symbolic-ref HEAD 2>/dev/null)
            set detached yes
            set branch (switch "$__fish_git_prompt_describe_style"
						case contains
							command git describe --contains HEAD
						case branch
							command git describe --contains --all HEAD
						case describe
							command git describe HEAD
						case default '*'
							command git describe --tags --exact-match HEAD
						end 2>/dev/null)
            if test $status -ne 0
                # Shorten the sha ourselves to 8 characters - this should be good for most repositories,
                # and even for large ones it should be good for most commits
                if set -q sha
                    set branch (string match -r '^.{8}' -- $sha)…
                else
                    set branch unknown
                end
            end
            set branch "($branch)"
        end
    end

    if test true = $inside_gitdir
        if test true = $bare_repo
            set bare "BARE:"
        else
            # Let user know they're inside the git dir of a non-bare repo
            set branch "GIT_DIR!"
        end
    end

    echo $operation
    echo $branch
    echo $detached
    echo $bare
end

function __fish_git_prompt_set_char
    set -l user_variable_name "$argv[1]"
    set -l char $argv[2]

    if set -q argv[3]
        and begin
            set -q __fish_git_prompt_show_informative_status
            or set -q __fish_git_prompt_use_informative_chars
        end
        set char $argv[3]
    end

    set -l variable _$user_variable_name
    set -l variable_done "$variable"_done

    if not set -q $variable
        set -g $variable (set -q $user_variable_name; and echo $$user_variable_name; or echo $char)
    end
end

function __fish_git_prompt_validate_chars --description "fish_git_prompt helper, checks char variables"

    __fish_git_prompt_set_char __fish_git_prompt_char_cleanstate '✔'
    __fish_git_prompt_set_char __fish_git_prompt_char_dirtystate '*' '✚'
    __fish_git_prompt_set_char __fish_git_prompt_char_invalidstate '#' '✖'
    __fish_git_prompt_set_char __fish_git_prompt_char_stagedstate '+' '●'
    __fish_git_prompt_set_char __fish_git_prompt_char_stashstate '$' '⚑'
    __fish_git_prompt_set_char __fish_git_prompt_char_stateseparator ' ' '|'
    __fish_git_prompt_set_char __fish_git_prompt_char_untrackedfiles '%' '…'
    __fish_git_prompt_set_char __fish_git_prompt_char_upstream_ahead '>' '↑'
    __fish_git_prompt_set_char __fish_git_prompt_char_upstream_behind '<' '↓'
    __fish_git_prompt_set_char __fish_git_prompt_char_upstream_diverged '<>'
    __fish_git_prompt_set_char __fish_git_prompt_char_upstream_equal '='
    __fish_git_prompt_set_char __fish_git_prompt_char_upstream_prefix ''

end

function __fish_git_prompt_set_color
    set -l user_variable_name "$argv[1]"

    set -l default default_done
    switch (count $argv)
        case 1 # No defaults given, use prompt color
            set default $___fish_git_prompt_color
            set default_done $___fish_git_prompt_color_done
        case 2 # One default given, use normal for done
            set default "$argv[2]"
            set default_done (set_color normal)
        case 3 # Both defaults given
            set default "$argv[2]"
            set default_done "$argv[3]"
    end

    set -l variable _$user_variable_name
    set -l variable_done "$variable"_done

    if not set -q $variable
        if test -n "$$user_variable_name"
            set -g $variable (set_color $$user_variable_name)
            set -g $variable_done (set_color normal)
        else
            set -g $variable $default
            set -g $variable_done $default_done
        end
    end
end


function __fish_git_prompt_validate_colors --description "fish_git_prompt helper, checks color variables"

    # Base color defaults to nothing (must be done first)
    __fish_git_prompt_set_color __fish_git_prompt_color '' ''

    # Normal colors
    __fish_git_prompt_set_color __fish_git_prompt_color_prefix
    __fish_git_prompt_set_color __fish_git_prompt_color_suffix
    __fish_git_prompt_set_color __fish_git_prompt_color_bare
    __fish_git_prompt_set_color __fish_git_prompt_color_merging
    __fish_git_prompt_set_color __fish_git_prompt_color_cleanstate
    __fish_git_prompt_set_color __fish_git_prompt_color_invalidstate
    __fish_git_prompt_set_color __fish_git_prompt_color_upstream

    # Colors with defaults with showcolorhints
    if set -q __fish_git_prompt_showcolorhints
        __fish_git_prompt_set_color __fish_git_prompt_color_flags (set_color --bold blue)
        __fish_git_prompt_set_color __fish_git_prompt_color_branch (set_color green)
        __fish_git_prompt_set_color __fish_git_prompt_color_dirtystate (set_color red)
        __fish_git_prompt_set_color __fish_git_prompt_color_stagedstate (set_color green)
    else
        __fish_git_prompt_set_color __fish_git_prompt_color_flags
        __fish_git_prompt_set_color __fish_git_prompt_color_branch
        __fish_git_prompt_set_color __fish_git_prompt_color_dirtystate $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
        __fish_git_prompt_set_color __fish_git_prompt_color_stagedstate $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
    end

    # Branch_detached has a default, but is only used with showcolorhints
    __fish_git_prompt_set_color __fish_git_prompt_color_branch_detached (set_color red)

    # Colors that depend on flags color
    __fish_git_prompt_set_color __fish_git_prompt_color_stashstate $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
    __fish_git_prompt_set_color __fish_git_prompt_color_untrackedfiles $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done

end

set -l varargs
for var in repaint describe_style show_informative_status use_informative_chars showdirtystate showstashstate showuntrackedfiles showupstream
    set -a varargs --on-variable __fish_git_prompt_$var
end
function __fish_git_prompt_reset $varargs --description "Event handler, resets prompt when functionality changes"
    if status --is-interactive
        if contains -- $argv[3] __fish_git_prompt_show_informative_status __fish_git_prompt_use_informative_chars
            # Clear characters that have different defaults with/without informative status
            for name in cleanstate dirtystate invalidstate stagedstate stashstate stateseparator untrackedfiles upstream_ahead upstream_behind
                set -e ___fish_git_prompt_char_$name
            end
            # Clear init so we reset the chars next time.
            set -e ___fish_git_prompt_init
        end
    end
end

set -l varargs
for var in '' _prefix _suffix _bare _merging _cleanstate _invalidstate _upstream _flags _branch _dirtystate _stagedstate _branch_detached _stashstate _untrackedfiles
    set -a varargs --on-variable __fish_git_prompt_color$var
end
set -a varargs --on-variable __fish_git_prompt_showcolorhints
function __fish_git_prompt_reset_color $varargs --description "Event handler, resets prompt when any color changes"
    if status --is-interactive
        set -e ___fish_git_prompt_init
        set -l var $argv[3]
        set -e _$var
        set -e _{$var}_done
        if test $var = __fish_git_prompt_color -o $var = __fish_git_prompt_color_flags -o $var = __fish_git_prompt_showcolorhints
            # reset all the other colors too
            for name in prefix suffix bare merging branch dirtystate stagedstate invalidstate stashstate untrackedfiles upstream flags
                set -e ___fish_git_prompt_color_$name
                set -e ___fish_git_prompt_color_{$name}_done
            end
        end
    end
end

set -l varargs
for var in cleanstate dirtystate invalidstate stagedstate stashstate stateseparator untrackedfiles upstream_ahead upstream_behind upstream_diverged upstream_equal upstream_prefix
    set -a varargs --on-variable __fish_git_prompt_char_$var
end
function __fish_git_prompt_reset_char $varargs --description "Event handler, resets prompt when any char changes"
    if status --is-interactive
        set -e ___fish_git_prompt_init
        set -e _$argv[3]
    end
end
