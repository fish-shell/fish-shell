# based off of the git-completion script that ships with git
#
# Written by Kevin Ballard <kevin@sb.org>
#
# This is heavily based off of the git-completion.bash script that ships with
# git, which is Copyright (C) 2006,2007 Shawn O. Pearce <spearce@spearce.org>.
# The act of porting the code, along with any new code, are Copyright (C) 2012
# Kevin Ballard <kevin@sb.org>.
#
# By virtue of being based on the git-completion.bash script, this script is
# distributed under the GNU General Public License, version 2.0.
#
# This script vends a function __fish_git_prompt which takes a format string,
# exactly how the bash script works. This can be used in your fish_prompt
# function.
#
# The behavior of __fish_git_prompt is very heavily based off of the bash
# script's __fish_git_prompt function. As such, usage and customization is very
# similar, although some extra flexibility is provided in this script.
#
# The argument to __fish_git_prompt will be displayed only if you are currently
# in a git repository. The %s token will be the name of the branch. If HEAD is
# not a branch, it attempts to show the relevant tag. The tag search is
# controlled by the __fish_git_prompt_describe_style variable, with the
# following values:
#     default (or unset)    Any tag that exactly matches HEAD
#     contains              Nearest annotated tag that contains HEAD
#     branch                Nearest tag/branch that contains HEAD
#     describe              Output of `git describe`
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
# untracked files, then a '%' will be shown next to the branch name.
#
# If you would like to see the difference between HEAD and its upstream, set
# __fish_git_prompt_showupstream to 'auto'. A "<" indicates you are behind, ">"
# indicates you are ahead, and "<>" indicates you have diverged. You can
# further control behavior by setting __fish_git_prompt_showupstream to a
# space-separated list of values:
#     verbose        show number of commits head/behind (+/-) upstream
#     legacy         don't use the '--count' option available in recent versions
#                    of git-rev-list
#     git            always compare HEAD to @{upstream}
#     svn            always compare HEAD to your SVN upstream
# By default, __fish_git_prompt will compare HEAD to your SVN upstream if it
# can find one, or @{upstream} otherwise. Once you have set
# __fish_git_prompt_showupstream, you can override it on a per-repository basis
# by setting the bash.showUpstream config variable. As before, this variable
# remains named 'bash' to preserve compatibility.
#
# This fish-compatible version of __fish_git_prompt includes some additional
# features on top of the above-documented bash-compatible features:
#
# The color for the branch name and each individual optional component can be
# specified using __fish_git_prompt_color_<name>, where <name> is 'prefix',
# 'suffix', 'bare', 'merging', 'branch', 'dirtystate', 'stagedstate',
# 'invalidstate', 'stashstate', 'untrackedfiles', and 'upstream'. The variable
# __fish_git_prompt_color is used for any component that does not have an
# individual color set. Colors are specified as arguments to `set_color`.
#
# The characters used for the optional features can be configured using
# __fish_git_prompt_char_<token>, where <token> is one of 'dirtystate',
# 'stagedstate', 'invalidstate', 'stashstate', 'untrackedfiles',
# 'upstream_equal', 'upstream_behind', 'upstream_ahead', and
# 'upstream_diverged'.

set -g ___fish_git_prompt_status_order stagedstate invalidstate dirtystate untrackedfiles

function __fish_git_prompt_show_upstream --description "Helper function for __fish_git_prompt"
	# Ask git-config for some config options
	set -l svn_remote
	set -l svn_prefix
	set -l upstream git
	set -l legacy
	set -l verbose
	set -l informative
	set -l svn_url_pattern
	set -l show_upstream $__fish_git_prompt_showupstream
	git config -z --get-regexp '^(svn-remote\..*\.url|bash\.showUpstream)$' ^/dev/null | tr '\0\n' '\n ' | while read -l key value
		switch $key
		case bash.showUpstream bash.showupstream
			set show_upstream $value
			test -n "$show_upstream"; or return
		case svn-remote.'*'.url
			set svn_remote $svn_remote $value
			set -l remote_prefix (/bin/sh -c 'echo "${1%.url}"' -- $key)
			set svn_prefix $svn_prefix $remote_prefix
			if test -n "$svn_url_pattern"
				set svn_url_pattern $svn_url_pattern"\|$value"
			else
				set svn_url_pattern $value
			end
			set upstream svn+git # default upstream is SVN if available, else git
		end
	end

	# parse configuration variables
	for option in $show_upstream
		switch $option
		case git svn
			set upstream $option
		case verbose
			set verbose 1
		case informative
			set informative 1
		case legacy
			set legacy 1
		end
	end

	# Find our upstream
	switch $upstream
	case git
		set upstream '@{upstream}'
	case svn\*
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
	end

	# Find how many commits we are ahead/behind our upstream
	set -l count
	if test -z "$legacy"
		set count (git rev-list --count --left-right $upstream...HEAD ^/dev/null)
	else
		# produce equivalent output to --count for older versions of git
		set -l os
		set -l commits (git rev-list --left-right $upstream...HEAD ^/dev/null; set os $status)
		if test $os -eq 0
			set -l behind (count (for arg in $commits; echo $arg; end | grep '^<'))
			set -l ahead (count (for arg in $commits; echo $arg; end | grep -v '^<'))
			set count "$behind	$ahead"
		else
			set count
		end
	end

	# calculate the result
	if test -n "$verbose"
		echo $count | read -l behind ahead
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_equal"
		case "0	*" # ahead of upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead"
		case "*	0" # behind upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind$behind"
		case '*' # diverged from upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_diverged$ahead-$behind"
		end
	else if test -n informative
		echo $count | read -l behind ahead
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
		case "0	*" # ahead of upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead"
		case "*	0" # behind upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind$behind"
		case '*' # diverged from upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead$___fish_git_prompt_char_upstream_behind$behind"
		end
	else
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_equal"
		case "0	*" # ahead of upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead$ahead"
		case "*	0" # behind upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind$behind"
		case '*' # diverged from upstream
			echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_diverged$ahead-$behind"
		end
	end
end

function __fish_git_prompt --description "Prompt function for Git"
	set -l git_dir (__fish_git_prompt_git_dir)
	test -n "$git_dir"; or return

	set -l r (__fish_git_prompt_current_operation $git_dir)
	set -l b (__fish_git_prompt_current_branch $git_dir)
	set -l w #dirty working directory
	set -l i #staged changes
	set -l s #stashes
	set -l u #untracked
	set -l c (__fish_git_prompt_current_branch_bare)
	set -l p #upstream
	set -l informative_status

	__fish_git_prompt_validate_chars

	if test "true" = (git rev-parse --is-inside-work-tree ^/dev/null)

		if test -n "$__fish_git_prompt_show_informative_status"
			set informative_status "|"(__fish_git_prompt_informative_status)
		else
			if test -n "$__fish_git_prompt_showdirtystate"
				set -l config (git config --bool bash.showDirtyState)
				if test "$config" != "false"
					set w (__fish_git_prompt_dirty)
					set i (__fish_git_prompt_staged)
				end
			end

			if test -n "$__fish_git_prompt_showstashstate"
				git rev-parse --verify refs/stash >/dev/null ^&1; and set s $___fish_git_prompt_char_stashstate
			end

			if test -n "$__fish_git_prompt_showuntrackedfiles"
				set -l files (git ls-files --others --exclude-standard)
				if test -n "$files"
					set u $___fish_git_prompt_char_untrackedfiles
				end
			end
		end

		if test -n "$__fish_git_prompt_showupstream"
			set p (__fish_git_prompt_show_upstream)
		end

	end

	__fish_git_prompt_validate_colors

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
	set b (/bin/sh -c 'echo "${1#refs/heads/}"' -- $b)
	if test -n "$b"
		set b "$___fish_git_prompt_color_branch$b$___fish_git_prompt_color_branch_done"
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
		set f " $f"
	end
	set -l format $argv[1]
	if test -z "$format"
		set format " (%s)"
	end

	printf "%s$format%s" "$___fish_git_prompt_color_prefix" "$___fish_git_prompt_color_prefix_done$c$b$f$r$p$informative_status$___fish_git_prompt_color_suffix" "$___git_ps_color_suffix_done"
end

### helper functions

function __fish_git_prompt_staged --description "__fish_git_prompt helper, tells whether or not the current branch has staged files"
	set -l staged

	if git rev-parse --quiet --verify HEAD >/dev/null
		git diff-index --cached --quiet HEAD --; or set staged $___fish_git_prompt_char_stagedstate
	else
		set staged $___fish_git_prompt_char_invalidstate
	end
	echo $staged
end

function __fish_git_prompt_dirty --description "__fish_git_prompt helper, tells whether or not the current branch has tracked, modified files"
	set -l dirty

	set -l os
	git diff --no-ext-diff --quiet --exit-code
	set os $status
	if test $os -ne 0
		set dirty $___fish_git_prompt_char_dirtystate
	end
	echo $dirty
end

function  __fish_git_prompt_informative_status

	set -l changedFiles (git diff --name-status | cut -c 1-2)
	set -l stagedFiles (git diff --staged --name-status | cut -c 1-2)

	set -l dirtystate (math (count $changedFiles) - (count (echo $changedFiles | grep "U")))
	set -l invalidstate (count (echo $stagedFiles | grep "U"))
	set -l stagedstate (math (count $stagedFiles) - $invalidstate)
	set -l untrackedfiles (count (git ls-files --others --exclude-standard))

	set -l info

	if [ (math $dirtystate + $invalidstate + $stagedstate + $untrackedfiles) = 0 ]
		set info $___fish_git_prompt_color_cleanstate$___fish_git_prompt_char_cleanstate$___fish_git_prompt_color_cleanstate_done
	else
		for i in $___fish_git_prompt_status_order
			 if [ $$i != "0" ]
				set -l color_var ___fish_git_prompt_color_$i
				set -l color_done_var ___fish_git_prompt_color_$i
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

function __fish_git_prompt_current_branch_bare --description "__fish_git_prompt helper, tells wheter or not the current branch is bare"
	set -l bare

	if test "true" = (git rev-parse --is-inside-git-dir ^/dev/null)
		if test "true" = (git rev-parse --is-bare-repository ^/dev/null)
			set bare "BARE:"
		end
	end
	echo $bare
end

function __fish_git_prompt_current_branch --description "__fish_git_prompt helper, returns the current Git branch"
	set -l git_dir $argv[1]
	set -l branch

	set -l os
	set branch (git symbolic-ref HEAD ^/dev/null; set os $status)
	if test $os -ne 0
		set branch (switch "$__fish_git_prompt_describe_style"
					case contains
						git describe --contains HEAD
					case branch
						git describe --contains --all HEAD
					case describe
						git describe HEAD
					case default '*'
						git describe --tags --exact-match HEAD
					end ^/dev/null; set os $status)
		if test $os -ne 0
			set branch (cut -c1-7 $git_dir/HEAD ^/dev/null; set os $status)
			if test $os -ne 0
				set branch unknown
			end
		end
		set branch "($branch)"
	end

	# Let user know they're inside the git dir of a non-bare repo
	if test "true" = (git rev-parse --is-inside-git-dir ^/dev/null)
		if test "false" = (git rev-parse --is-bare-repository ^/dev/null)
			set branch "GIT_DIR!"
		end
	end
	echo $branch
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

function __fish_git_prompt_set_char
	set -l user_variable_name "$argv[1]"
	set -l char $argv[2]
	set -l user_variable $$user_variable_name

	set -l variable _$user_variable_name
	set -l variable_done "$variable"_done

	if not set -q $variable
		set -g $variable (set -q $user_variable_name; and echo $user_variable; or echo $char)
	end

end

function __fish_git_prompt_validate_chars --description "__fish_git_prompt helper, checks char variables"

	__fish_git_prompt_set_char __fish_git_prompt_char_cleanstate  			'.'
	__fish_git_prompt_set_char __fish_git_prompt_char_dirtystate  			'*'
	__fish_git_prompt_set_char __fish_git_prompt_char_stagedstate  			'+'
	__fish_git_prompt_set_char __fish_git_prompt_char_invalidstate  		'#'
	__fish_git_prompt_set_char __fish_git_prompt_char_stashstate  			'$'
	__fish_git_prompt_set_char __fish_git_prompt_char_untrackedfiles  		'%'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_equal  		'='
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_behind 		'<'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_ahead  		'>'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_diverged  	'<>'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_prefix  		' '

end

function __fish_git_prompt_set_color
	set -l user_variable_name "$argv[1]"
	set -l user_variable $$user_variable_name
	set -l user_variable_bright

	if test (count $user_variable) -eq 2
		set user_variable_bright $user_variable[2]
		set user_variable $user_variable[1]
	end

	set -l variable _$user_variable_name
	set -l variable_done "$variable"_done

	if not set -q $variable
		if test -n "$user_variable"
			if test -n "$user_variable_bright"
				set -g $variable (set_color -o $user_variable)
			else
				set -g $variable (set_color $user_variable)
			end
			set -g $variable_done (set_color normal)
		else
			set -g $variable ''
			set -g $variable_done ''
		end
	end

end

function __fish_git_prompt_validate_colors --description "__fish_git_prompt helper, checks color variables"

	__fish_git_prompt_set_color __fish_git_prompt_color
	__fish_git_prompt_set_color __fish_git_prompt_color_prefix
	__fish_git_prompt_set_color __fish_git_prompt_color_suffix
	__fish_git_prompt_set_color __fish_git_prompt_color_bare
	__fish_git_prompt_set_color __fish_git_prompt_color_merging
	__fish_git_prompt_set_color __fish_git_prompt_color_branch
	__fish_git_prompt_set_color __fish_git_prompt_color_cleanstate
	__fish_git_prompt_set_color __fish_git_prompt_color_dirtystate
	__fish_git_prompt_set_color __fish_git_prompt_color_stagedstate
	__fish_git_prompt_set_color __fish_git_prompt_color_invalidstate
	__fish_git_prompt_set_color __fish_git_prompt_color_stashstate
	__fish_git_prompt_set_color __fish_git_prompt_color_untrackedfiles
	__fish_git_prompt_set_color __fish_git_prompt_color_upstream

end

set -l varargs
for var in repaint describe_style showdirtystate showstashstate showuntrackedfiles showupstream
	set varargs $varargs --on-variable __fish_git_prompt_$var
end
function __fish_git_prompt_repaint $varargs --description "Event handler, repaints prompt when functionality changes"
	if status --is-interactive
		commandline -f repaint ^/dev/null
	end
end

set -l varargs
for var in '' _prefix _suffix _bare _merging _branch _dirtystate _stagedstate _invalidstate _stashstate _untrackedfiles _upstream
	set varargs $varargs --on-variable __fish_git_prompt_color$var
end
function __fish_git_prompt_repaint_color $varargs --description "Event handler, repaints prompt when any color changes"
	if status --is-interactive
		set -l var $argv[3]
		set -e _$var
		set -e _{$var}_done
		if test $var = __fish_git_prompt_color
			# reset all the other colors too
			for name in prefix suffix bare merging branch dirtystate stagedstate invalidstate stashstate untrackedfiles upstream
				set -e ___fish_git_prompt_color_$name
				set -e ___fish_git_prompt_color_{$name}_done
			end
		end
		commandline -f repaint ^/dev/null
	end
end
set -l varargs
for var in dirtystate stagedstate invalidstate stashstate untrackedfiles upstream_equal upstream_behind upstream_ahead upstream_diverged
	set varargs $varargs --on-variable __fish_git_prompt_char_$var
end
function __fish_git_prompt_repaint_char $varargs --description "Event handler, repaints prompt when any char changes"
	if status --is-interactive
		set -e _$argv[3]
		commandline -f repaint ^/dev/null
	end
end
