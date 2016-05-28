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
# This script vends a function __fish_git_prompt which takes a format string,
# exactly how the bash script works. This can be used in your fish_prompt
# function.
#
# The behavior of __fish_git_prompt is very heavily based off of the bash
# script's __git_ps1 function. As such, usage and customization is very
# similar, although some extra features are provided in this script.
# Due to differences between bash and fish, the PROMPT_COMMAND style where
# passing two or three arguments causes the fucnction to set PS1 is not
# supported.  More information on the additional features is found after the
# bash-compatable documentation.
#
# The argument to __fish_git_prompt will be displayed only if you are currently
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
#     legacy         don't use the '--count' option available in recent versions
#                    of git-rev-list
#     git            always compare HEAD to @{upstream}
#     svn            always compare HEAD to your SVN upstream
#     none           disables (fish only, useful with show_informative_status)
#
# By default, __fish_git_prompt will compare HEAD to your SVN upstream if it
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
# __fish_git_prompt_showcolorhints to a nonempty value.  The default colors are
# based on the colored output of "git status -sb"


# __fish_git_prompt includes some additional features on top of the
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

function __fish_git_prompt_show_upstream --description "Helper function for __fish_git_prompt"
	set -l show_upstream $__fish_git_prompt_showupstream
	set -l svn_prefix # For better SVN upstream information
	set -l informative

	set -l svn_url_pattern
	set -l count
	set -l upstream git
	set -l legacy
	set -l verbose
	set -l name

	# Default to informative if show_informative_status is set
	if test -n "$__fish_git_prompt_show_informative_status"
		set informative 1
	end

	set -l svn_remote
	# get some config options from git-config
	command git config -z --get-regexp '^(svn-remote\..*\.url|bash\.showupstream)$' ^/dev/null | tr '\0\n' '\n ' | while read -l key value
		switch $key
		case bash.showupstream
			set show_upstream $value
			test -n "$show_upstream"; or return
		case svn-remote.'*'.url
			set svn_remote $svn_remote $value
			# Avoid adding \| to the beginning to avoid needing #?? later
			if test -n "$svn_url_pattern"
				set svn_url_pattern $svn_url_pattern"\\|$value"
			else
				set svn_url_pattern $value
			end
			set upstream svn+git # default upstream is SVN if available, else git

			# Save the config key (without .url) for later use
			set -l remote_prefix (echo $key | sed 's/\.url$//')
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
		case legacy
			set legacy 1
			set -e informative
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
		# get the upstream from the 'git-svn-id: ...' in a commit message
		# (git-svn uses essentially the same procedure internally)
		set -l svn_upstream (git log --first-parent -1 --grep="^git-svn-id: \($svn_url_pattern\)" ^/dev/null)
		if test (count $svn_upstream) -ne 0
			echo $svn_upstream[-1] | read -l __ svn_upstream __
			set svn_upstream (echo $svn_upstream | sed 's/@.*//')
			set -l cur_prefix
			for i in (seq (count $svn_remote))
				set -l remote $svn_remote[$i]
				set -l mod_upstream (echo $svn_upstream | sed "s|$remote||")
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
				set upstream (echo $svn_upstream | sed 's|/branches||; s|/||g')

				# Use fetch config to fix upstream
				set -l fetch_val (command git config "$cur_prefix".fetch)
				if test -n "$fetch_val"
					set -l IFS :
					echo "$fetch_val" | read -l trunk pattern
					set upstream (echo $pattern | sed -e "s|/$trunk\$||") /$upstream
				end
			end
		else if test $upstream = svn+git
			set upstream '@{upstream}'
		end
	end

	# Find how many commits we are ahead/behind our upstream
	if test -z "$legacy"
		set count (command git rev-list --count --left-right $upstream...HEAD ^/dev/null)
	else
		# produce equivalent output to --count for older versions of git
		set -l os
		set -l commits (command git rev-list --left-right $upstream...HEAD ^/dev/null; set os $status)
		if test $os -eq 0
			set -l behind (count (for arg in $commits; echo $arg; end | string match -r '^<'))
			set -l ahead (count (for arg in $commits; echo $arg; end | string match -r -v '^<'))
			set count "$behind	$ahead"
		else
			set count
		end
	end

	# calculate the result
	if test -n "$verbose"
		# Verbose has a space by default
		set -l prefix "$___fish_git_prompt_char_upstream_prefix"
		# Using two underscore version to check if user explicitly set to nothing
		if not set -q __fish_git_prompt_char_upstream_prefix
			set -l prefix " "
		end

		echo $count | read -l behind ahead
		switch "$count"
		case '' # no upstream
		case "0	0" # equal to upstream
			echo "$prefix$___fish_git_prompt_char_upstream_equal"
		case "0	*" # ahead of upstream
			echo "$prefix$___fish_git_prompt_char_upstream_ahead$ahead"
		case "*	0" # behind upstream
			echo "$prefix$___fish_git_prompt_char_upstream_behind$behind"
		case '*' # diverged from upstream
			echo "$prefix$___fish_git_prompt_char_upstream_diverged$ahead-$behind"
		end
		if test -n "$count" -a -n "$name"
			echo " "(command git rev-parse --abbrev-ref "$upstream" ^/dev/null)
		end
	else if test -n "$informative"
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
                        echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_ahead"
		case "*	0" # behind upstream
                        echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_behind"
		case '*' # diverged from upstream
                        echo "$___fish_git_prompt_char_upstream_prefix$___fish_git_prompt_char_upstream_diverged"
		end
	end
end

function __fish_git_prompt --description "Prompt function for Git"
	# If git isn't installed, there's nothing we can do
	# Return 1 so the calling prompt can deal with it
	if not command -s git >/dev/null
		return 1
	end
	set -l repo_info (command git rev-parse --git-dir --is-inside-git-dir --is-bare-repository --is-inside-work-tree HEAD ^/dev/null)
	test -n "$repo_info"; or return

	set -l git_dir         $repo_info[1]
	set -l inside_gitdir   $repo_info[2]
	set -l bare_repo       $repo_info[3]
	set -l inside_worktree $repo_info[4]
	set -q repo_info[5]; and set -l sha $repo_info[5]

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

	__fish_git_prompt_validate_chars
	__fish_git_prompt_validate_colors

	set -l space "$___fish_git_prompt_color$___fish_git_prompt_char_stateseparator$___fish_git_prompt_color_done"

	if test "true" = $inside_worktree
		if test -n "$__fish_git_prompt_show_informative_status"
			set informative_status "$space"(__fish_git_prompt_informative_status)
		else
			if test -n "$__fish_git_prompt_showdirtystate"
				set -l config (command git config --bool bash.showDirtyState)
				if test "$config" != "false"
					set w (__fish_git_prompt_dirty)
					set i (__fish_git_prompt_staged $sha)
				end
			end

			if test -n "$__fish_git_prompt_showstashstate" -a -r $git_dir/refs/stash
				set s $___fish_git_prompt_char_stashstate
			end

			if test -n "$__fish_git_prompt_showuntrackedfiles"
				set -l config (command git config --bool bash.showUntrackedFiles)
				if test "$config" != false
					if command git ls-files --others --exclude-standard --error-unmatch -- '*' >/dev/null ^/dev/null
						set u $___fish_git_prompt_char_untrackedfiles
					end
				end
			end
		end

		if test -n "$__fish_git_prompt_showupstream" -o "$__fish_git_prompt_show_informative_status"
			set p (__fish_git_prompt_show_upstream)
		end
	end

	set -l branch_color $___fish_git_prompt_color_branch
	set -l branch_done  $___fish_git_prompt_color_branch_done
	if test -n "$__fish_git_prompt_showcolorhints"
		if test $detached = yes
			set branch_color $___fish_git_prompt_color_branch_detached
			set branch_done  $___fish_git_prompt_color_branch_detached_done
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
	set b (echo $b | sed 's|refs/heads/||')
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

	printf "%s$format%s" "$___fish_git_prompt_color_prefix" "$___fish_git_prompt_color_prefix_done$c$b$f$r$p$informative_status$___fish_git_prompt_color_suffix" "$___git_ps_color_suffix_done"
end

### helper functions

function __fish_git_prompt_staged --description "__fish_git_prompt helper, tells whether or not the current branch has staged files"
	set -l sha $argv[1]

	set -l staged

	if test -n "$sha"
		command git diff-index --cached --quiet HEAD --; or set staged $___fish_git_prompt_char_stagedstate
	else
		set staged $___fish_git_prompt_char_invalidstate
	end
	echo $staged
end

function __fish_git_prompt_dirty --description "__fish_git_prompt helper, tells whether or not the current branch has tracked, modified files"
	set -l dirty

	set -l os
	command git diff --no-ext-diff --quiet --exit-code
	set os $status
	if test $os -ne 0
		set dirty $___fish_git_prompt_char_dirtystate
	end
	echo $dirty
end

set -g ___fish_git_prompt_status_order stagedstate invalidstate dirtystate untrackedfiles

function  __fish_git_prompt_informative_status

	set -l changedFiles (command git diff --name-status | cut -c 1-2)
	set -l stagedFiles (command git diff --staged --name-status | cut -c 1-2)

	set -l dirtystate (math (count $changedFiles) - (count (echo $changedFiles | grep "U")))
	set -l invalidstate (count (echo $stagedFiles | grep "U"))
	set -l stagedstate (math (count $stagedFiles) - $invalidstate)
	set -l untrackedfiles (count (command git ls-files --others --exclude-standard))

	set -l info

	if [ (math $dirtystate + $invalidstate + $stagedstate + $untrackedfiles) = 0 ]
		set info $___fish_git_prompt_color_cleanstate$___fish_git_prompt_char_cleanstate$___fish_git_prompt_color_cleanstate_done
	else
		for i in $___fish_git_prompt_status_order
			 if [ $$i != "0" ]
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
function __fish_git_prompt_operation_branch_bare --description "__fish_git_prompt helper, returns the current Git operation and branch"
	# This function is passed the full repo_info array
	set -l git_dir         $argv[1]
	set -l inside_gitdir   $argv[2]
	set -l bare_repo       $argv[3]
	set -q argv[5]; and set -l sha $argv[5]

	set -l branch
	set -l operation
	set -l detached no
	set -l bare
	set -l step
	set -l total
	set -l os

	if test -d $git_dir/rebase-merge
		set branch (cat $git_dir/rebase-merge/head-name ^/dev/null)
		set step (cat $git_dir/rebase-merge/msgnum ^/dev/null)
		set total (cat $git_dir/rebase-merge/end ^/dev/null)
		if test -f $git_dir/rebase-merge/interactive
			set operation "|REBASE-i"
		else
			set operation "|REBASE-m"
		end
	else
		if test -d $git_dir/rebase-apply
			set step (cat $git_dir/rebase-apply/next ^/dev/null)
			set total (cat $git_dir/rebase-apply/last ^/dev/null)
			if test -f $git_dir/rebase-apply/rebasing
				set branch (cat $git_dir/rebase-apply/head-name ^/dev/null)
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
		set branch (command git symbolic-ref HEAD ^/dev/null; set os $status)
		if test $os -ne 0
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
						end ^/dev/null; set os $status)
			if test $os -ne 0
				# Shorten the sha ourselves to 8 characters - this should be good for most repositories,
				# and even for large ones it should be good for most commits
				if set -q sha
					set branch (string match -r '^.{8}' -- $sha)...
				else
					set branch unknown
				end
			end
			set branch "($branch)"
		end
	end

	if test "true" = $inside_gitdir
		if test "true" = $bare_repo
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
	set -l user_variable $$user_variable_name

	if test (count $argv) -ge 3
		if test -n "$__fish_git_prompt_show_informative_status"
			set char $argv[3]
		end
	end

	set -l variable _$user_variable_name
	set -l variable_done "$variable"_done

	if not set -q $variable
		set -g $variable (set -q $user_variable_name; and echo $user_variable; or echo $char)
	end
end

function __fish_git_prompt_validate_chars --description "__fish_git_prompt helper, checks char variables"

	__fish_git_prompt_set_char __fish_git_prompt_char_cleanstate        '✔'
	__fish_git_prompt_set_char __fish_git_prompt_char_dirtystate        '*' '✚'
	__fish_git_prompt_set_char __fish_git_prompt_char_invalidstate      '#' '✖'
	__fish_git_prompt_set_char __fish_git_prompt_char_stagedstate       '+' '●'
	__fish_git_prompt_set_char __fish_git_prompt_char_stashstate        '$'
	__fish_git_prompt_set_char __fish_git_prompt_char_stateseparator    ' ' '|'
	__fish_git_prompt_set_char __fish_git_prompt_char_untrackedfiles    '%' '…'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_ahead    '>' '↑'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_behind   '<' '↓'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_diverged '<>'
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_equal    '='
	__fish_git_prompt_set_char __fish_git_prompt_char_upstream_prefix   ''

end

function __fish_git_prompt_set_color
	set -l user_variable_name "$argv[1]"
	set -l user_variable $$user_variable_name
	set -l user_variable_bright

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
		if test -n "$user_variable"
			set -g $variable (set_color $user_variable)
			set -g $variable_done (set_color normal)
		else
			set -g $variable $default
			set -g $variable_done $default_done
		end
	end

end


function __fish_git_prompt_validate_colors --description "__fish_git_prompt helper, checks color variables"

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
	if test -n "$__fish_git_prompt_showcolorhints"
		__fish_git_prompt_set_color __fish_git_prompt_color_flags       (set_color --bold blue)
		__fish_git_prompt_set_color __fish_git_prompt_color_branch      (set_color green)
		__fish_git_prompt_set_color __fish_git_prompt_color_dirtystate  (set_color red)
		__fish_git_prompt_set_color __fish_git_prompt_color_stagedstate (set_color green)
	else
		__fish_git_prompt_set_color __fish_git_prompt_color_flags
		__fish_git_prompt_set_color __fish_git_prompt_color_branch
		__fish_git_prompt_set_color __fish_git_prompt_color_dirtystate  $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
		__fish_git_prompt_set_color __fish_git_prompt_color_stagedstate $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
	end

	# Branch_detached has a default, but is only used with showcolorhints
	__fish_git_prompt_set_color __fish_git_prompt_color_branch_detached (set_color red)

	# Colors that depend on flags color
	__fish_git_prompt_set_color __fish_git_prompt_color_stashstate      $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done
	__fish_git_prompt_set_color __fish_git_prompt_color_untrackedfiles  $___fish_git_prompt_color_flags $___fish_git_prompt_color_flags_done

end

set -l varargs
for var in repaint describe_style show_informative_status showdirtystate showstashstate showuntrackedfiles showupstream
	set varargs $varargs --on-variable __fish_git_prompt_$var
end
function __fish_git_prompt_repaint $varargs --description "Event handler, repaints prompt when functionality changes"
	if status --is-interactive
		if test $argv[3] = __fish_git_prompt_show_informative_status
			# Clear characters that have different defaults with/without informative status
			for name in cleanstate dirtystate invalidstate stagedstate stateseparator untrackedfiles upstream_ahead upstream_behind
				set -e ___fish_git_prompt_char_$name
			end
		end

		commandline -f repaint ^/dev/null
	end
end

set -l varargs
for var in '' _prefix _suffix _bare _merging _cleanstate _invalidstate _upstream _flags _branch _dirtystate _stagedstate _branch_detached _stashstate _untrackedfiles
	set varargs $varargs --on-variable __fish_git_prompt_color$var
end
set varargs $varargs --on-variable __fish_git_prompt_showcolorhints
function __fish_git_prompt_repaint_color $varargs --description "Event handler, repaints prompt when any color changes"
	if status --is-interactive
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
		commandline -f repaint ^/dev/null
	end
end

set -l varargs
for var in cleanstate dirtystate invalidstate stagedstate stashstate stateseparator untrackedfiles upstream_ahead upstream_behind upstream_diverged upstream_equal upstream_prefix
	set varargs $varargs --on-variable __fish_git_prompt_char_$var
end
function __fish_git_prompt_repaint_char $varargs --description "Event handler, repaints prompt when any char changes"
	if status --is-interactive
		set -e _$argv[3]
		commandline -f repaint ^/dev/null
	end
end
