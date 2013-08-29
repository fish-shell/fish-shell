set -g __git_prompt_color_upstream (set_color red)
set -g __git_prompt_color_dirty (set_color yellow)
set -g __git_prompt_color_invalid (set_color red)
set -g __git_prompt_color_staged (set_color green)
set -g __git_prompt_color_untracked (set_color normal)
set -g __git_prompt_color_stashes (set_color purple)
set -g __git_prompt_color_clean (set_color -o green)
set -g __git_prompt_color_branch (set_color normal)
set -g __git_prompt_color_current_operation (set_color normal)

set -g __git_prompt_char_upstream_ahead ↑
set -g __git_prompt_char_upstream_behind ↓
set -g __git_prompt_char_dirty ✚
set -g __git_prompt_char_invalid ✖
set -g __git_prompt_char_staged ●
set -g __git_prompt_char_untracked …
set -g __git_prompt_char_stashes 𝄐
set -g __git_prompt_char_clean ✔

function __fish_git_prompt_new --description "Prompt function for Git"
	set -l git_dir (__fish_git_prompt_git_dir)
	test -n "$git_dir"; or return

	set -l current_operation (__fish_git_prompt_current_operation $git_dir)
	set -l branch (__fish_git_prompt_current_branch $git_dir)
	set -l bare_branch (__fish_git_prompt_current_branch_bare)

	set -l nr_of_dirty_files
	set -l nr_of_staged_files
	set -l nr_of_invalid_files
	set -l stashes
	set -l nr_of_untracked_files
	set -l upstream


	if test "true" = (git rev-parse --is-inside-work-tree ^/dev/null)
		set nr_of_dirty_files (__fish_git_prompt_part dirty (__fish_git_nr_of_dirty_files))
		set nr_of_invalid_files (__fish_git_prompt_part invalid (__fish_git_nr_of_invalid_files))
		set nr_of_staged_files (__fish_git_prompt_part staged (__fish_git_nr_of_staged_files))
		set nr_of_untracked_files (__fish_git_prompt_part untracked (__fish_git_nr_of_untracked_files))
		if __fish_git_has_stashes
			set stashes (__fish_git_prompt_part stashes)
		end
		set upstream (__fish_git_prompt_show_upstream)
	end

	set branch $__git_prompt_color_branch$branch(set_color normal)

# FIXME I don't think I'm using this atm. Is 'branch' and 'bare branch' mutually exclusive?
	if test -n "$bare_branch"
		set c "$__git_prompt_color_bare$bare_branch$___fish_git_prompt_color_bare_done"
	end

	if test -n "$current_operation"
		set current_operation $__git_prompt_color_current_operation$current_operation(set_color normal)
	end

	if test -n "$upsteam"
		set upstream $__git_prompt_color_upstream$upstream(set_color normal)
	else
		set upstream ""
	end

	# Formatting
	set -l concatenated_status "$nr_of_dirty_files$nr_of_staged_files$nr_of_untracked_files$stashes"
	if test -z "$concatenated_status"
		set concatenated_status -o $__git_prompt_color_clean$__git_prompt_char_clean(set_color normal)
	end

	printf "%s (%s)%s" (set_color normal) "$bare_branch$branch$upstream|$concatenated_status$current_operation" (set_color normal)
end

### helper functions
function __fish_git_prompt_part --description "Output prompt parts"
	echo $argv | read -l key nr_of_files
	set -l color __git_prompt_color_$key
	set -l char __git_prompt_char_$key

	if test -n "$nr_of_files"
		test $nr_of_files -eq 0; and return
	end

	echo -n $$color
	echo -n $$char
	echo -n $nr_of_files
	set_color normal
end

function __fish_git_nr_of_dirty_files --description "Returns the number of tracked, changed files"
	count (git diff --name-status | cut -c 1-2)
end

function __fish_git_nr_of_staged_files --description "Returns the number of staged files"
	count (git diff --staged --name-status | cut -c 1-2 | grep -v "U")
end

function __fish_git_nr_of_invalid_files --description "Returns the number of files with conflicts"
	count (git diff --staged --name-status | cut -c 1-2 | grep "U")
end

function __fish_git_nr_of_untracked_files --description "Returns the number of files not yet added to the repository"
	count (git ls-files --others --exclude-standard)
end

function __fish_git_has_stashes --description "Returns the 'stash' character if git dir has any number of stashes"
	git rev-parse --verify refs/stash >/dev/null ^&1
end

# FIXME this is some complex stuff. Could easily be split in a function for grabbing the upstream name and the divergence counts
function __fish_git_prompt_show_upstream --description "__fish_git_prompt helper, returns symbols encoding whether or not and if so how much the upstream and local have diverged"
	# Default assume we have a regular Git upstream
	set -l upstream '@{upstream}'

	# Test if we have a SVN remote
	git config -z --get-regexp '^(svn-remote\..*\.url)$' ^/dev/null; set os $status
	if test $os -eq 0
		set upstream (__fish_git_prompt_svn_upstream)
	end

	# Find how many commits we are ahead/behind our upstream
	set -l count (git rev-list --count --left-right $upstream...HEAD ^/dev/null)

	# calculate the result
	echo $count | read -l behind ahead
	switch "$count"
	case '' # no upstream
	case "0	0" # equal to upstream
	case "0	*" # ahead of upstream
		echo "$__git_prompt_char_upstream_ahead$ahead"
	case "*	0" # behind upstream
		echo "$__git_prompt_char_upstream_behind$behind"
	case '*' # diverged from upstream
		echo "$__git_prompt_char_upstream_ahead$ahead$__git_prompt_char_upstream_behind$behind"
	end
end

function __fish_git_prompt_svn_upstream --description "__fish_git_prompt helper, returns the SVN upstream"
	set -l upstream

	set -l svn_remote
	set -l svn_prefix
	set -l svn_url_pattern

	git config -z --get-regexp '^(svn-remote\..*\.url)$' ^/dev/null | tr '\0\n' '\n ' | while read -l key value
		switch $key
		case svn-remote.'*'.url
			set svn_remote $svn_remote $value
			set -l remote_prefix (/bin/sh -c 'echo "${1%.url}"' -- $key)
			set svn_prefix $svn_prefix $remote_prefix
			if test -n "$svn_url_pattern"
				set svn_url_pattern $svn_url_pattern"\|$value"
			else
				set svn_url_pattern $value
			end
		end
	end

	# Find our upstream
	# get the upstream from the 'git-svn-id: ...' in a commit message
	# (git-svn uses essentially the same procedure internally)
	set -l svn_upstream (git log --first-parent -1 --grep="^git-svn-id: \($svn_url_pattern\)" ^/dev/null)
	if test (count $svn_upstream) -ne 0
		echo $svn_upstream[-1] | read -l _ svn_upstream _
		set svn_upstream (/bin/sh -c 'echo "${1%@*}"' -- $svn_upstream)
		set -l cur_prefix
		for i in (seq (count $svn_remote))
			set -l remote $svn_remote[$i]
			set -l mod_upstream (/bin/sh -c 'echo "${1#$2}"' -- $svn_upstream $remote)
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
			set upstream (/bin/sh -c 'val=${1#/branches}; echo "${val#/}"' -- $svn_upstream)
			set -l fetch_val (git config "$cur_prefix".fetch)
			if test -n "$fetch_val"
				set -l IFS :
				echo "$fetch_val" | read -l trunk pattern
				set upstream (/bin/sh -c 'echo "${1%/$2}"' -- $pattern $trunk)/$upstream
			end
		end
	else if test $upstream = svn+git
		set upstream '@{upstream}'
	end

	echo $upstream
end

function __fish_git_prompt_current_branch_bare --description "__fish_git_prompt helper, tells wheter or not the current branch is bare"
	if test "true" = (git rev-parse --is-inside-git-dir ^/dev/null) -a "true" = (git rev-parse --is-bare-repository ^/dev/null)
		echo bare "BARE:"
	end
end

function __fish_git_prompt_current_branch --description "__fish_git_prompt helper, returns the current Git branch"
	set branch (git symbolic-ref --short --quiet HEAD ^/dev/null)
	echo $branch
end

function debugger --on-event print
  echo DEBUG: $argv
end

function __fish_git_prompt_current_operation --description "__fish_git_prompt helper, returns the current Git operation being performed"
	set -l operation

	set -l git_dir $argv[1]
	if test -f $git_dir/rebase-merge/interactive
		set operation "|REBASE-i"
	else if test -d $git_dir/rebase-merge
		set operation "|REBASE-m"
	else
		if test -d $git_dir/rebase-apply
			if test -f $git_dir/rebase-apply/rebasing
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
		else if test -f $git_dir/BISECT_LOG
			set operation "|BISECTING"
		end
	end
	echo $operation
end

function __fish_git_prompt_git_dir --description "__fish_git_prompt helper, returns .git dir if any"
	echo (git rev-parse --git-dir ^/dev/null)
end
