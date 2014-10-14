# Git info functions give you status info about git
#
# This is split out from the prompt function because:
# - it's evaluated once
# - it can be used in either prompt or right prompt
# - it's useful outside the prompt (scripts)
# - IOW it separates concerns of state evaluation and use
#
# Some vars control advanced but time-consuming behavior, set those vars
# universally or globally, depending on your use case:
#
#		set -U GIT_INFO_SHOWDIRTYSTATE 1
#		set -U GIT_INFO_SHOWSTASHSTATE 1
#		set -U GIT_INFO_SHOWUNTRACKEDFILES 1
#
# Example prompt definition usage:
#
# function fish_prompt
#		...
#		# evaluate git state as lazily as possible
#		__git_info_gitdir # ideally only called when pwd changes
#		[ -n $GIT_INFO_GITDIR ]; and __git_info_vars # sets state vars
#		...
#
#		# echo your prompt
#		pwd # or whatever
#
#		# add git prompt info if vars set
#		if [ -n "$GIT_INFO_STATUS" ]
#			# set your prompt as you wish according to GIT_INFO_*
#			# see available vars and values at the very end
#			#
#			# a colorful example:
#			set_color blue
#			echo " $GIT_INFO_REF "
#			set_color red
#			contains t $GIT_INFO_STATUS; and echo -n "!"
#			set_color green
#			contains s $GIT_INFO_STATUS; and echo -n "±"
#			set_color yellow
#			contains u $GIT_INFO_STATUS; and echo -n "≠"
#			set_color normal
#			contains n $GIT_INFO_STATUS; and echo -n "∅"
#			contains h $GIT_INFO_STATUS; and echo -n "↰"
#		 end
#
#		 # echo whatever else you need in your prompt
#		 echo -n ' $ '
# end

function __git_info_rebase_merge_head --description 'find head being merged'
	cat "{$argv[1]}/rebase-merge/head-name"
end

function __git_info_describe --description "describe according to GIT_INFO_DESCRIBE_STYLE"
	switch "$GIT_INFO_DESCRIBE_STYLE"
		case contains
			git describe --contains HEAD
		case branch
			git describe --contains --all HEAD
		case describe
			git describe HEAD
		case '*' default
			git describe --exact-match HEAD
	end 2>/dev/null
end

function __git_info_ref --description "get current branch or ref"
	set -l ref (git symbolic-ref HEAD 2>/dev/null); or \
	set -l ref (__git_info_describe); or \
	set -l ref (git rev-parse --short HEAD); or \
	set -l ref "unknown"

	echo $ref | sed 's#^refs/heads/##'
end

function __git_info_vars --description "compute git status and set environment variables"
	set -l g "$GIT_INFO_GITDIR"

	if [ -z "$g" ]
		set -e GIT_INFO_STATUS
		set -e GIT_INFO_REF
		set -e GIT_INFO_SUBJECT
		set -e GIT_INFO_TOPLEVEL
		set -e GIT_INFO_NAME
		set -e GIT_INFO_PREFIX
		return
	end

	set -l rebase 0
	set -l interactive 0
	set -l apply 0
	set -l merge 0
	set -l bisect 0
	set -l subject ""
	set -l ref ""
	set -l gitdir 0
	set -l bare 0
	set -l work 0
	set -l staged 0
	set -l unstaged 0
	set -l new 0
	set -l untracked 0
	set -l stashed 0

	set -l toplevel ""
	set -l prefix ""
	set -l name ""

	# assess position in repository
	[ "true" = (git rev-parse --is-inside-git-dir 2>/dev/null) ]; and set gitdir 1
	[ "$gitdir" -eq 1 -a "true" = (git rev-parse --is-bare-repository  2>/dev/null) ]; and set bare 1
	[ "$gitdir" -eq 0 -a "true" = (git rev-parse --is-inside-work-tree 2>/dev/null) ]; and set work 1

	# gitdir corner case
	if [ "$g" = '.' ]
		if [ "(basename "$PWD")" = ".git" ]
			# inside .git: not a bare repository!
			# weird: --is-bare-repository returns true regardless
			set bare 0
			set g "$PWD"
		else
			# really a bare repository
			set bare 1
			set g "$PWD"
		end
	end

	# make relative path absolute
	[ (echo "$g" | sed 's#^/##') = "$g" ]; and set g "$PWD/$g"
	set g (echo "$g" | sed 's#/\$##')

	# find base dir (toplevel)
	[ "$bare" -eq 1 ]; and set toplevel "$g"
	[ "$bare" -eq 0 ]; and set toplevel (dirname "$g")

	# find relative path within toplevel
	set prefix (echo "$PWD" | sed "s#^$toplevel##")
	set prefix (echo "$prefix" | sed 's#^/##')
	[ -z "$prefix" ]; and set prefix '.' # toplevel == prefix

	# get the current branch, or whatever describes HEAD
	set ref (__git_info_ref)

	# get name
	set name (basename "$toplevel")

	# evaluate action
	if [ -d "$g/rebase-merge" ]
		set rebase 1
		set merge 1
		set subject (__git_info_rebase_merge_head $g)
	end
	if [ $rebase -eq 1 -a -f "$g/rebase-merge/interactive" ]
		set interactive 1
		set merge 0
	end
	if [ -d "$g/rebase-apply" ]
		set rebase 1
		set apply 1
	end
	[ $apply  -eq 1 -a -f "$g/rebase-apply/applying" ]; and set rebase 0
	[ $apply  -eq 1 -a -f "$g/rebase-apply/rebasing" ]; and set apply 0
	[ $rebase -eq 0 -a -f "$g/MERGE_HEAD" ]; and set merge 1
	[ $rebase -eq 0 -a -f "$g/BISECT_LOG" ]; and set bisect 1

	# working directory status
	if [ $work -eq 1 ]
		## dirtiness, if config allows it
		if [	-n "$GIT_INFO_SHOWDIRTYSTATE" ]
			# unstaged files
			git diff --no-ext-diff --ignore-submodules --quiet --exit-code; or set unstaged 1

			if git rev-parse --quiet --verify HEAD >/dev/null
				# staged files
				git diff-index --cached --quiet --ignore-submodules HEAD --; or set staged 1
			else
				# no current commit, we're a freshly init'd repo
				set new 1
			end
		end

		## stash status
		if [ -n "$GIT_INFO_SHOWSTASHSTATE" ]
			git rev-parse --verify refs/stash >/dev/null 2>&1; and set stashed 1
		end

		## untracked files
		if [ -n "$GIT_INFO_SHOWUNTRACKEDFILES" ]
			set -l untracked_files (git ls-files --others --exclude-standard)
			[ -n "$untracked_files" ]; and set untracked 1
		end
	end

	# build global environment variables
	set -g GIT_INFO_STATUS ""
	[ $rebase      -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "R"
	[ $interactive -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "i"
	[ $apply       -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "A"
	[ $merge       -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "M"
	[ $bisect      -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "B"
	[ $gitdir      -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "g"
	[ $bare        -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "b"
	[ $work        -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "w"
	[ $staged      -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "s"
	[ $unstaged    -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "u"
	[ $new         -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "n"
	[ $untracked   -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "t"
	[ $stashed     -eq 1 ]; and set GIT_INFO_STATUS $GIT_INFO_STATUS "h"
	set -g GIT_INFO_REF "$ref"
	set -g GIT_INFO_SUBJECT "$subject"
	set -g GIT_INFO_TOPLEVEL "$toplevel"
	set -g GIT_INFO_NAME "$name"
	set -g GIT_INFO_PREFIX "$prefix"
end
