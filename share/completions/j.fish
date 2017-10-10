function __history_completions --argument limit
	if echo $limit | string match -q ""
	    set limit 25
	end

	set -l tokens (commandline --current-process --tokenize)
	history --prefix (commandline) | string replace -r \^$tokens[1]\\s\* "" | head -n$limit
end

complete -k -c j -a '(__history_completions 25)' -f
