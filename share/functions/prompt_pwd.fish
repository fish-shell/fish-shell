function prompt_pwd --description "Print the current working directory, shortened to fit the prompt"
	# This allows overriding fish_prompt_pwd_dir_length from the outside (global or universal) without leaking it
	set -q fish_prompt_pwd_dir_length; or set -l fish_prompt_pwd_dir_length 1

	# Replace $HOME with "~"
	set realhome ~
	set -l tmp (string replace -r '^'"$realhome"'($|/)' '~$1' $PWD)

	if [ $fish_prompt_pwd_dir_length -eq 0 ]
		echo $tmp
	else
		# Shorten to at most $fish_prompt_pwd_dir_length characters per directory
		string replace -ar '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*/' '$1/' $tmp
	end
end
