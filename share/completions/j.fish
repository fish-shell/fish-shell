function __history_completions
	set -l tokens (commandline --current-process --tokenize)
	history --prefix (commandline) | string replace -r \^$tokens[1]\\s\* ""
end

complete -k -c j -a '(__history_completions)' -f
