__fish_make_completion_signals

for i in $__kill_signals
	set number (echo $i | cut -d " " -f 1)
	set name (echo $i | cut -d " " -f 2)
	complete -c kill -o $number -d $name
	complete -c kill -o $name -d $name
	complete -c kill -o s -x -a \"$number\tSend\ $name\ signal\"
	complete -c kill -o s -x -a \"$name\tSend\ $name\ signal\"
end

complete -c kill -xa '(__fish_complete_pids)'

