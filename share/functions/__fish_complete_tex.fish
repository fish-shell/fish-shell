
function __fish_complete_tex -d "Common completions for all tex commands"

	complete -c $argv -o help --description "Display help and exit"
	complete -c $argv -o version --description "Display version and exit"
	complete -c $argv -x -a "(
		__fish_complete_suffix (commandline -ct) .tex '(La)TeX file'
	)"

    complete -c $argv -o     file-line-error -d "Show errors in style file:line"
    complete -c $argv -o  no-file-line-error -d "Show errors not in style file:line"
    complete -c $argv -o  halt-on-error      -d "Stop processing at the first error"
    complete -c $argv -o  interaction=batchmode   -d "Set interaction mode"
    complete -c $argv -o  interaction=nonstopmode -d "Set interaction mode"
    complete -c $argv -o  interaction=scrollmode -d "Set interaction mode"
    complete -c $argv -o  interaction=errorstopmode -d "Set interaction mode"
    complete -c $argv -o  output-directory=  -x -a "(__fish_complete_directories (commandline -ct))" -d "Output directory"
    complete -c $argv -o     shell-escape    -d "Enable \write18{SHELL COMMAND}"
    complete -c $argv -o  no-shell-escape    -d "Disable \write18{SHELL COMMAND}"
    complete -c $argv -o  src-specials       -d "Insert source specials into the DVI file"
end
