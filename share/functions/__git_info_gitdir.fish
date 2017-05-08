# Git info functions related to working dir
#
# This is split out from the info functions because it's evaluated once,
# and possibly only on pwd change

# Adapted from the official git bash prompt
# TODO: Since git ~2.x and submodules, .git can be a file
function __gitdir --description "get a folder's git dir (defaults to current)"
	if [ -z "$argv" ]
		if [ -n "$__git_dir" ]
			echo "$__git_dir"
		else if [ -d .git ]
			echo .git
		else
			git rev-parse --git-dir 2>/dev/null
		end
	else if [ -d "$argv[1]/.git" ]
		echo "$argv[1]/.git"
	else
		echo "$argv[1]"
	end
end

# Eval current dir's gitdir and cache it in an env var for performance on reuse
# Best used only when pwd changes, to minimize stat calls
function __git_info_gitdir --description "set GIT_INFO_GITDIR to gitdir path if available, otherwise reset it"
	set -l g (__gitdir)

	# are we in a git repo?
	if [ -z "$g" ]
		set -e GIT_INFO_GITDIR
	else
		set -g GIT_INFO_GITDIR "$g"
	end
end
