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
# If you would like to see if there're untracked files, then you can set
# __fish_git_prompt_showuntrackedfiles to a nonempty value. If there're
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

function __fish_git_prompt_show_upstream --description "Helper function for __fish_git_prompt"
	# Ask git-config for some config options
	set -l svn_remote
	set -l svn_prefix
	set -l upstream git
	set -l legacy
	set -l verbose
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
			set svn_upstream (echo $svn_upstream | sed 's/@.*//')
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
				if test (echo $svn_upstream | cut -c 1) = /
					set upstream (echo $svn_upstream | cut -c 2-)
				else
					set upstream $svn_upstream
				end
				set -l fetch_val (git config "$cur_prefix".fetch)
				if test -n "$fetch_val"
					set -l IFS :
					echo "$fetch_val" | read -l trunk pattern
					set upstream (/bin/sh -c 'echo "${1%/$2}"' -- $pattern $trunk)/$upstream
				end
			end
		else
			if test $upstream = svn+git
				set upstream '@{upstream}'
			end
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
	if test -z "$verbose"
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
			echo $___fish_git_prompt_char_upstream_equal
		case "0	*" # ahead of upstream
			echo $___fish_git_prompt_char_upstream_ahead
		case "*	0" # behind upstream
			echo $___fish_git_prompt_char_upstream_behind
		case '*' # diverged from upstream
			echo $___fish_git_prompt_char_upstream_diverged
		end
	else
		echo $count | read -l behind ahead
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
			echo " u="
		case "0	*" # ahead of upstream
			echo " u+$ahead"
		case "*	0" # behind upstream
			echo " u-$behind"
		case '*' # diverged from upstream
			echo " u+$ahead-$behind"
		end
	end
end

function __fish_git_prompt --description "Prompt function for Git"
	# find the enclosing git dir
	set -l git_dir
	if test -d .git
		set git_dir .git
	else
		set git_dir (git rev-parse --git-dir ^/dev/null)
	end
	test -n "$git_dir"; or return

	set -l r
	set -l b
	if test -f $git_dir/rebase-merge/interactive
		set r "|REBASE-i"
		set b (cat $git_dir/rebase-merge/head-name)
	else; if test -d $git_dir/rebase-merge
			set r "|REBASE-m"
			set b (cat $git_dir/rebase-merge/head-name)
		else
			if test -d $git_dir/rebase-apply
				if test -f $git_dir/rebase-apply/rebasing
					set r "|REBASE"
				else; if test -f $git_dir/rebase-apply/applying
						set r "|AM"
					else
						set r "|AM/REBASE"
					end
				end
			else; if test -f $git_dir/MERGE_HEAD
					set r "|MERGING"
				else; if test -f $git_dir/CHERRY_PICK_HEAD
						set r "|CHERRY-PICKING"
					else; if test -f $git_dir/BISECT_LOG
							set r "|BISECTING"
						end
					end
				end
			end

			set -l os
			set b (git symbolic-ref HEAD ^/dev/null; set os $status)
			if test $os -ne 0
				set b (switch "$__fish_git_prompt_describe_style"
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
					set b (cut -c1-7 $git_dir/HEAD ^/dev/null; set os $status)
					if test $os -ne 0
						set b unknown
					end
				end
				set b "($b)"
			end
		end
	end

	set -l w
	set -l i
	set -l s
	set -l u
	set -l c
	set -l p

	__fish_git_prompt_validate_chars

	if test "true" = (git rev-parse --is-inside-git-dir ^/dev/null)
		if test "true" = (git rev-parse --is-bare-repository ^/dev/null)
			set c "BARE:"
		else
			set b "GIT_DIR!"
		end
	else; if test "true" = (git rev-parse --is-inside-work-tree ^/dev/null)
			if test -n "$__fish_git_prompt_showdirtystate"
				set -l config (git config --bool bash.showDirtyState)
				if test "$config" != "false"
					git diff --no-ext-diff --quiet --exit-code; or set w $___fish_git_prompt_char_dirtystate
					if git rev-parse --quiet --verify HEAD >/dev/null
						git diff-index --cached --quiet HEAD --; or set i $___fish_git_prompt_char_stagedstate
					else
						set i $___fish_git_prompt_char_invalidstate
					end
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

			if test -n "$__fish_git_prompt_showupstream"
				set p (__fish_git_prompt_show_upstream)
			end
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
	set -l f "$w$i$s$u"
	set -l format $argv[1]
	if test -z "$format"
		set format " (%s)"
	end
	if test -n "$f"
		set f " $f"
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
	printf "%s$format%s" "$___fish_git_prompt_color_prefix" "$___fish_git_prompt_color_prefix_done$c$b$f$r$p$___fish_git_prompt_color_suffix" "$___git_ps_color_suffix_done"
end

### helper functions

function __fish_git_prompt_validate_chars --description "__fish_git_prompt helper, checks char variables"
	if not set -q ___fish_git_prompt_char_dirtystate
		set -g ___fish_git_prompt_char_dirtystate (set -q __fish_git_prompt_char_dirtystate; and echo $__fish_git_prompt_char_dirtystate; or echo '*')
	end
	if not set -q ___fish_git_prompt_char_stagedstate
		set -g ___fish_git_prompt_char_stagedstate (set -q __fish_git_prompt_char_stagedstate; and echo $__fish_git_prompt_char_stagedstate; or echo '+')
	end
	if not set -q ___fish_git_prompt_char_invalidstate
		set -g ___fish_git_prompt_char_invalidstate (set -q __fish_git_prompt_char_invalidstate; and echo $__fish_git_prompt_char_invalidstate; or echo '#')
	end
	if not set -q ___fish_git_prompt_char_stashstate
		set -g ___fish_git_prompt_char_stashstate (set -q __fish_git_prompt_char_stashstate; and echo $__fish_git_prompt_char_stashstate; or echo '$')
	end
	if not set -q ___fish_git_prompt_char_untrackedfiles
		set -g ___fish_git_prompt_char_untrackedfiles (set -q __fish_git_prompt_char_untrackedfiles; and echo $__fish_git_prompt_char_untrackedfiles; or echo '%')
	end
	if not set -q ___fish_git_prompt_char_upstream_equal
		set -g ___fish_git_prompt_char_upstream_equal (set -q __fish_git_prompt_char_upstream_equal; and echo $__fish_git_prompt_char_upstream_equal; or echo '=')
	end
	if not set -q ___fish_git_prompt_char_upstream_behind
		set -g ___fish_git_prompt_char_upstream_behind (set -q __fish_git_prompt_char_upstream_behind; and echo $__fish_git_prompt_char_upstream_behind; or echo '<')
	end
	if not set -q ___fish_git_prompt_char_upstream_ahead
		set -g ___fish_git_prompt_char_upstream_ahead (set -q __fish_git_prompt_char_upstream_ahead; and echo $__fish_git_prompt_char_upstream_ahead; or echo '>')
	end
	if not set -q ___fish_git_prompt_char_upstream_diverged
		set -g ___fish_git_prompt_char_upstream_diverged (set -q __fish_git_prompt_char_upstream_diverged; and echo $__fish_git_prompt_char_upstream_diverged; or echo '<>')
	end
end

function __fish_git_prompt_validate_colors --description "__fish_git_prompt helper, checks color variables"
	if not set -q ___fish_git_prompt_color
		if test -n "$__fish_git_prompt_color"
			set -g ___fish_git_prompt_color (set_color $__fish_git_prompt_color)
			set -g ___fish_git_prompt_color_done (set_color normal)
		else
			set -g ___fish_git_prompt_color ''
			set -g ___fish_git_prompt_color_done ''
		end
	end
	if not set -q ___fish_git_prompt_color_prefix
		if test -n "$__fish_git_prompt_color_prefix"
			set -g ___fish_git_prompt_color_prefix (set_color $__fish_git_prompt_color_prefix)
			set -g ___fish_git_prompt_color_prefix_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_prefix $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_prefix_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_suffix
		if test -n "$__fish_git_prompt_color_suffix"
			set -g ___fish_git_prompt_color_suffix (set_color $__fish_git_prompt_color_suffix)
			set -g ___fish_git_prompt_color_suffix_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_suffix $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_suffix_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_bare
		if test -n "$__fish_git_prompt_color_bare"
			set -g ___fish_git_prompt_color_bare (set_color $__fish_git_prompt_color_bare)
			set -g ___fish_git_prompt_color_bare_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_bare $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_bare_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_merging
		if test -n "$__fish_git_prompt_color_merging"
			set -g ___fish_git_prompt_color_merging (set_color $__fish_git_prompt_color_merging)
			set -g ___fish_git_prompt_color_merging_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_merging $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_merging_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_branch
		if test -n "$__fish_git_prompt_color_branch"
			set -g ___fish_git_prompt_color_branch (set_color $__fish_git_prompt_color_branch)
			set -g ___fish_git_prompt_color_branch_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_branch $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_branch_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_dirtystate
		if test -n "$__fish_git_prompt_color_dirtystate"
			set -g ___fish_git_prompt_color_dirtystate (set_color $__fish_git_prompt_color_dirtystate)
			set -g ___fish_git_prompt_color_dirtystate_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_dirtystate $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_dirtystate_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_stagedstate
		if test -n "$__fish_git_prompt_color_stagedstate"
			set -g ___fish_git_prompt_color_stagedstate (set_color $__fish_git_prompt_color_stagedstate)
			set -g ___fish_git_prompt_color_stagedstate_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_stagedstate $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_stagedstate_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_invalidstate
		if test -n "$__fish_git_prompt_color_invalidstate"
			set -g ___fish_git_prompt_color_invalidstate (set_color $__fish_git_prompt_color_invalidstate)
			set -g ___fish_git_prompt_color_invalidstate_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_invalidstate $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_invalidstate_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_stashstate
		if test -n "$__fish_git_prompt_color_stashstate"
			set -g ___fish_git_prompt_color_stashstate (set_color $__fish_git_prompt_color_stashstate)
			set -g ___fish_git_prompt_color_stashstate_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_stashstate $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_stashstate_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_untrackedfiles
		if test -n "$__fish_git_prompt_color_untrackedfiles"
			set -g ___fish_git_prompt_color_untrackedfiles (set_color $__fish_git_prompt_color_untrackedfiles)
			set -g ___fish_git_prompt_color_untrackedfiles_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_untrackedfiles $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_untrackedfiles_done $___fish_git_prompt_color_done
		end
	end
	if not set -q ___fish_git_prompt_color_upstream
		if test -n "$__fish_git_prompt_color_upstream"
			set -g ___fish_git_prompt_color_upstream (set_color $__fish_git_prompt_color_upstream)
			set -g ___fish_git_prompt_color_upstream_done (set_color normal)
		else
			set -g ___fish_git_prompt_color_upstream $___fish_git_prompt_color
			set -g ___fish_git_prompt_color_upstream_done $___fish_git_prompt_color_done
		end
	end
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
