# based off of the git-prompt script that ships with git
# hence licensed under GPL version 2 (like the rest of fish).
#
# Written by Lily Ballard and updated by Brian Gernhardt and fish contributors
#
# This is based on git's git-prompt.bash script, Copyright (C) 2006,2007 Shawn O. Pearce <spearce@spearce.org>.
# The act of porting the code, along with any new code, are Copyright (C) 2012 Lily Ballard.

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
    if contains -- "$__fish_git_prompt_show_informative_status" yes true 1
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

# Decide if git is safe to run.
# On Darwin, git is pre-installed as a stub, which will pop a dialog if you run it.
if string match -q Darwin -- (uname) && string match -q /usr/bin/git -- (command -s git) && type -q xcode-select && type -q xcrun
    if not xcode-select --print-path &>/dev/null
        # Only the stub git is installed.
        # Do not try to run it.
        function __fish_git_prompt_ready
            return 1
        end
    else
        # git is installed, but on the first run it may be very slow as xcrun needs to populate the cache.
        # Kick it off in the background to populate the cache.
        /bin/sh -c '( /usr/bin/git --version; touch /tmp/__fish_git_ready ) >/dev/null 2>&1 &'
        function __fish_git_prompt_ready
            path is /tmp/__fish_git_ready || return 1
            # git is ready, erase the function.
            functions -e __fish_git_prompt_ready
            return 0
        end
    end
end

function fish_git_prompt --description "Prompt function for Git"
    # If git isn't installed, there's nothing we can do
    # Return 1 so the calling prompt can deal with it
    if not command -sq git
        return 1
    end
    # Fail if __fish_git_prompt_ready is defined and fails.
    if functions -q __fish_git_prompt_ready && not __fish_git_prompt_ready
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
    set -l dirtystate #dirty working directory
    set -l stagedstate #staged changes
    set -l invalidstate #staged changes
    set -l stashstate #stashes
    set -l untrackedfiles #untracked
    set -l c $rbc[4] # bare repository
    set -l p #upstream
    set -l informative_status

    set -q __fish_git_prompt_status_order
    or set -g __fish_git_prompt_status_order stagedstate invalidstate dirtystate untrackedfiles stashstate

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
    set -l informative
    set -l dirty
    set -l untracked
    command git config -z --get-regexp 'bash\.(showInformativeStatus|showDirtyState|showUntrackedFiles)' 2>/dev/null | while read -lz key value
        switch $key
            case bash.showinformativestatus
                set informative $value
            case bash.showdirtystate
                set dirty $value
            case bash.showuntrackedfiles
                set untracked $value
        end
    end

    # If we don't print these, there is no need to compute them. Note: For now, staged and dirty are coupled.
    if not set -q dirty[1]
        contains -- "$__fish_git_prompt_showdirtystate" yes true 1
        and set dirty true
    end
    contains dirtystate $__fish_git_prompt_status_order || contains stagedstate $__fish_git_prompt_status_order
    or set dirty false

    if not set -q untracked[1]
        contains -- "$__fish_git_prompt_showuntrackedfiles" yes true 1
        and set untracked true
    end
    contains untrackedfiles $__fish_git_prompt_status_order
    or set untracked false

    if test true = $inside_worktree
        # Use informative status if it has been enabled locally, or it has been
        # enabled globally (via the fish variable) and dirty or untracked are not false.
        #
        # This is to allow overrides for the repository.
        if test "$informative" = true
            or begin
                contains -- "$__fish_git_prompt_show_informative_status" yes true 1
                and test "$dirty" != false
            end
            set informative_status (untracked=$untracked __fish_git_prompt_informative_status $git_dir)
            if test -n "$informative_status"
                set informative_status "$space$informative_status"
            end
        else
            if not test "$dirty" = true; and test "$untracked" = true
                # Only untracked, ls-files is faster.
                command git -c core.fsmonitor= ls-files --others --exclude-standard --directory --no-empty-directory --error-unmatch -- :/ >/dev/null 2>&1
                and set untrackedfiles 1
            else if test "$dirty" = true
                # With both dirty and untracked, git status is ~10% faster.
                # With just dirty, it's ~20%.
                set -l opt -uno
                test "$untracked" = true; and set opt -unormal
                # Don't use `--ignored=no`; it was introduced in Git 2.16, from January 2018
                # Ignored files are omitted by default
                set -l stat (command git -c core.fsmonitor= status --porcelain -z $opt | string split0)

                set dirtystate (string match -qr '^.[ACDMRTU]' -- $stat; and echo 1)
                if test -n "$sha"
                    set stagedstate (string match -qr '^[ACDMRTU].' -- $stat; and echo 1)
                else
                    set invalidstate 1
                end

                test "$untracked" = true
                and set untrackedfiles (string match -qr '\?\?' -- $stat; and echo 1)
            end

            if contains -- "$__fish_git_prompt_showstashstate" yes true 1
                and test -r $git_dir/logs/refs/stash
                # If we have informative status but don't want to actually
                # *compute* the informative status, we might still count the stash.
                if contains -- "$__fish_git_prompt_show_informative_status" yes true 1
                    set stashstate (count < $git_dir/logs/refs/stash)
                else
                    set stashstate 1
                end
            end
        end

        # (showupstream has a variety of options, not just bool)
        if set -q __fish_git_prompt_showupstream
            or contains -- "$__fish_git_prompt_show_informative_status" yes true 1
            set p (__fish_git_prompt_show_upstream)
        end
    end

    set -l branch_color $___fish_git_prompt_color_branch
    set -l branch_done $___fish_git_prompt_color_branch_done
    if contains -- "$__fish_git_prompt_showcolorhints" yes true 1
        if test $detached = yes
            set branch_color $___fish_git_prompt_color_branch_detached
            set branch_done $___fish_git_prompt_color_branch_detached_done
        else if test -n "$dirtystate$untrackedfiles"; and set -q __fish_git_prompt_color_branch_dirty
            set branch_color (set_color $__fish_git_prompt_color_branch_dirty)
            set branch_done (set_color $__fish_git_prompt_color_branch_dirty_done)
        else if test -n "$stagedstate"; and set -q __fish_git_prompt_color_branch_staged
            set branch_color (set_color $__fish_git_prompt_color_branch_staged)
            set branch_done (set_color $__fish_git_prompt_color_branch_staged_done)
        end
    end

    set -l f ""
    for i in $__fish_git_prompt_status_order
        if test -n "$$i"
            set -l color_var ___fish_git_prompt_color_$i
            set -l color_done_var ___fish_git_prompt_color_{$i}_done
            set -l symbol_var ___fish_git_prompt_char_$i

            set -l color $$color_var
            set -l color_done $$color_done_var
            set -l symbol $$symbol_var

            # If we count some things, print the number
            # This won't be done if we actually do the full informative status
            # because that does the printing.
            contains -- "$__fish_git_prompt_show_informative_status" yes true 1
            and set f "$f$color$symbol$$i$color_done"
            or set f "$f$color$symbol$color_done"
        end
    end

    set b (string replace refs/heads/ '' -- $b)
    if string match -qr '^\d+$' "$__fish_git_prompt_shorten_branch_len"
        set -q __fish_git_prompt_shorten_branch_char_suffix
        and set -l char -c "$__fish_git_prompt_shorten_branch_char_suffix"
        set b (string shorten -m "$__fish_git_prompt_shorten_branch_len" $char -- "$b")
    end
    if test -n "$b"
        set b "$branch_color$b$branch_done"
        if test -z "$dirtystate$untrackedfiles$stagedstate"; and test -n "$___fish_git_prompt_char_cleanstate"
            and not contains -- "$__fish_git_prompt_show_informative_status" yes true 1
            set b "$b$___fish_git_prompt_color_cleanstate$___fish_git_prompt_char_cleanstate$___fish_git_prompt_color_cleanstate_done"
        end
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
    # If we have state, a bare repo or upstream difference, add a separator.
    # merging is already separate.
    if test -n "$f$c$p"
        set f "$space$f"
    end
    set -l format $argv[1]
    if test -z "$format"
        set format " (%s)"
    end

    printf "%s$format%s" "$___fish_git_prompt_color_prefix" "$___fish_git_prompt_color_prefix_done$c$b$f$r$p$informative_status$___fish_git_prompt_color_suffix" "$___fish_git_prompt_color_suffix_done"
end

### helper functions

function __fish_git_prompt_informative_status
    set -l stashstate 0
    set -l stashfile "$argv[1]/logs/refs/stash"
    if contains -- "$__fish_git_prompt_showstashstate" yes true 1; and test -e "$stashfile"
        set stashstate (count < $stashfile)
    end

    # If we're not told to show untracked files, we don't.
    # If we are, we still use the "normal" mode because it's a lot faster,
    # and it's unlikely anyone cares about the number of files if it's *all* of the files
    # in that directory.
    set -l untr -uno
    test "$untracked" = true
    and set untr -unormal

    # Use git status --porcelain.
    # The v2 format is better, but we don't actually care in this case.
    set -l stats (string sub -l 2 (git -c core.fsmonitor= status --porcelain -z $untr | string split0))
    set -l invalidstate (string match -r '^UU' $stats | count)
    set -l stagedstate (string match -r '^[ACDMRT].' $stats | count)
    set -l dirtystate (string match -r '^.[ACDMRT]' $stats | count)
    set -l untrackedfiles (string match -r '^\?\?' $stats | count)

    set -l info

    if test "$dirtystate$invalidstate$stagedstate$untrackedfiles$stashstate" = 00000
        if test -n "$___fish_git_prompt_char_cleanstate"
            set info $___fish_git_prompt_color_cleanstate$___fish_git_prompt_char_cleanstate$___fish_git_prompt_color_cleanstate_done
        end
    else
        for i in $__fish_git_prompt_status_order
            if test $$i != 0
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
                # No need for an ellipsis.
                if set -q sha
                    set branch (string shorten -m8 -c "" -- $sha)
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
            contains -- "$__fish_git_prompt_show_informative_status" yes true 1
            or contains -- "$__fish_git_prompt_use_informative_chars" yes true 1
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
    # cleanstate is only defined with actual informative status.
    contains -- "$__fish_git_prompt_show_informative_status" yes true 1
    and __fish_git_prompt_set_char __fish_git_prompt_char_cleanstate '✔'
    or __fish_git_prompt_set_char __fish_git_prompt_char_cleanstate ''

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
    if contains -- "$__fish_git_prompt_showcolorhints" yes true 1
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

function __fish_git_prompt_reset -a type -a op -a var --description "Event handler, resets prompt when functionality changes" \
    --on-variable=__fish_git_prompt_{show_informative_status,use_informative_chars}
    if status --is-interactive
        # Clear characters that have different defaults with/without informative status
        set -e ___fish_git_prompt_char_{name,cleanstate,dirtystate,invalidstate,stagedstate,stashstate,stateseparator,untrackedfiles,upstream_ahead,upstream_behind}
        # Clear init so we reset the chars next time.
        set -e ___fish_git_prompt_init
    end
end

function __fish_git_prompt_reset_color -a type -a op -a var --description "Event handler, resets prompt when any color changes" \
    --on-variable=__fish_git_prompt_color{'',_prefix,_suffix,_bare,_merging,_cleanstate,_invalidstate,_upstream,_flags,_branch,_dirtystate,_stagedstate,_branch_detached,_stashstate,_untrackedfiles} --on-variable=__fish_git_prompt_showcolorhints
    if status --is-interactive
        set -e _$var
        set -e _{$var}_done
        set -e ___fish_git_prompt_init
        if contains -- $var __fish_git_prompt_color __fish_git_prompt_color_flags __fish_git_prompt_showcolorhints
            # reset all the other colors too
            set -e ___fish_git_prompt_color_{prefix,suffix,bare,merging,branch,dirtystate,stagedstate,invalidstate,stashstate,untrackedfiles,upstream,flags}{,_done}
        end
    end
end

function __fish_git_prompt_reset_char -a type -a op -a var --description "Event handler, resets prompt when any char changes" \
    --on-variable=__fish_git_prompt_char_{cleanstate,dirtystate,invalidstate,stagedstate,stashstate,stateseparator,untrackedfiles,upstream_ahead,upstream_behind,upstream_diverged,upstream_equal,upstream_prefix}
    if status --is-interactive
        set -e ___fish_git_prompt_init
        set -e _$var
    end
end
