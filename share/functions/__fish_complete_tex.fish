
function __fish_complete_tex -d "Common completions for all tex commands"

	complete -c $argv -o help --description "Display help and exit"
	complete -c $argv -o version --description "Display version and exit"
	complete -c $argv -x -a "(
		__fish_complete_suffix (commandline -ct) .tex '(La)TeX file'
	)"

end
