# Completions for the kill command

set -l signals

if kill -L ^/dev/null >/dev/null

	# Debian and some related systems use 'kill -L' to write out a numbered list 
	# of signals. Use this to complete on both number _and_ on signal name.

	complete -c kill -s L --description "List codes and names of available signals"

	set signals (kill -L | sed -e 's/\([0-9][0-9]*\)  *\([A-Z,0-9][A-Z,0-9]*\)/\1 \2\n/g;s/ +/ /g' | sed -e 's/^ //' | sgrep -E '^[^ ]+')

else
	
	# Posix systems print out the name of a signal using 'kill -l
	# SIGNUM', so we use this instead.

	complete -c kill -s l --description "List names of available signals"

	for i in (seq 31)
		set signals $signals $i" "(kill -l $i)
	end

end

for i in $signals
	set number (echo $i | cut -d " " -f 1)
	set name (echo $i | cut -d " " -f 2)
	complete -c kill -o $number -d $name
	complete -c kill -o $name -d $name
	complete -c kill -o s -x -a \"$number\tSend\ $name\ signal\"
	complete -c kill -o s -x -a \"$name\tSend\ $name\ signal\" 
end


complete -c kill -xa '(__fish_complete_pids)'

