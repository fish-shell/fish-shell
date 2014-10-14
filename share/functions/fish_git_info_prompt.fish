# Set the default prompt command. Make sure that every terminal escape
# string has a newline before and after, so that fish will know how
# long it is.
#
# Some vars control advanced but time-consuming git behavior, set those vars
# universally or globally, depending on your use case:
#
#		set -U GIT_INFO_SHOWDIRTYSTATE 1
#		set -U GIT_INFO_SHOWSTASHSTATE 1
#		set -U GIT_INFO_SHOWUNTRACKEDFILES 1

function fish_prompt --description "Write out the prompt with git info"
	# Just calculate these once, to save a few cycles when displaying the prompt
	if not set -q __fish_prompt_hostname
		set -g __fish_prompt_hostname (hostname|cut -d . -f 1)
	end

	if not set -q __fish_prompt_normal
		set -g __fish_prompt_normal (set_color normal)
	end

		# Evaluate git state only once, and only if needed
		__git_info_gitdir	# ideally only called when pwd changes
		[ -n $GIT_INFO_GITDIR ]; and __git_info_vars	# be lazy: sets vars only if git

	switch $USER

		case root toor

		if not set -q __fish_prompt_cwd
			if set -q fish_color_cwd_root
				set -g __fish_prompt_cwd (set_color $fish_color_cwd_root)
			else
				set -g __fish_prompt_cwd (set_color $fish_color_cwd)
			end
		end

		echo -n -s "$USER" @ "$__fish_prompt_hostname" ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal" '# '

		case '*'

		if not set -q __fish_prompt_cwd
			set -g __fish_prompt_cwd (set_color $fish_color_cwd)
		end

		echo -n -s "$USER" @ "$__fish_prompt_hostname" ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal"

		# add git prompt info
		if [ -n "$GIT_INFO_STATUS" ]
			set_color blue
			echo -n " $GIT_INFO_REF"

			set -l vcs_status ""
			contains h $GIT_INFO_STATUS; and set vcs_status "$vcs_status""↰"
			contains t $GIT_INFO_STATUS; and set vcs_status "$vcs_status""!"
			contains u $GIT_INFO_STATUS; and set vcs_status "$vcs_status""≠"
			contains s $GIT_INFO_STATUS; and set vcs_status "$vcs_status""±"
			contains n $GIT_INFO_STATUS; and set vcs_status "$vcs_status""∅"
			set_color red
			[ -n "$vcs_status" ]; and echo -n " $vcs_status"

			set -l action ""
			contains R $GIT_INFO_STATUS; and set action "$action rebase"
			contains i $GIT_INFO_STATUS; and set action "$action-i"
			contains A $GIT_INFO_STATUS; and set action "$action apply"
			contains M $GIT_INFO_STATUS; and set action "$action merge"
			contains B $GIT_INFO_STATUS; and set action "$action bisect"
			set_color yellow
			[ -n "$action" ]; and echo -n " $action"
		end

		# close prompt
		set_color normal
		echo -n '> '
	end
end
