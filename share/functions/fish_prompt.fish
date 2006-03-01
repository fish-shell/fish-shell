# Set the default prompt command. Make sure that every terminal escape
# string has a newline before and after, so that fish will know how
# long it is.

function fish_prompt -d (N_ "Write out the prompt")

	# Just calculate these once, to save a few cycles when displaying the prompt
	if not set -q __fish_prompt_hostname
		set -g __fish_prompt_hostname (hostname|cut -d . -f 1)
	end
	
	if not set -q __fish_prompt_normal
		set -g __fish_prompt_normal (set_color normal)
	end
	
	set -l prompt_color (set_color $fish_color_cwd)

	printf '%s@%s %s%s%s> \n' $USER $__fish_prompt_hostname "$prompt_color" (prompt_pwd) "$__fish_prompt_normal"
end

