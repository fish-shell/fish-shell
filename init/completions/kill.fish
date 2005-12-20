# Completions for the kill command

if kill -L ^/dev/null >/dev/null

	# Debian and some related systems use 'kill -L' to write out a numbered list 
	# of signals. Use this to complete on both number _and_ on signal name.

	complete -c kill -s L -d "List codes and names of available signals"

	set -- signals (kill -L | sed -e 's/\([0-9][0-9]*\)  *([A-Z,0-9][A-Z,0-9]*\)/\1 \2\n/g;s/ +/ /g' | sed -e 's/^ //' | grep -E '^[^ ]+')
	for i in $signals
		set -- number (echo $i | cut -d " " -f 1)
		set -- name (echo $i | cut -d " " -f 2)
		complete -c kill -o $number -d $name
		complete -c kill -o $name -d $name
		complete -c kill -o s -x -a \"$number\tSend\ $name\ signal\" -d "Send specified signal"
		complete -c kill -o s -x -a \"$name\tSend\ $name\ signal\" -d "Send specified signal"
	end

else
	
	# Other systems print out a list of signals in an unspecified way on 
	# 'kill -l'. We complete with anything that looks like a signal name.

	complete -c kill -s l -d "List codes and names of available signals"

	for i in (kill -l|tr \ \t \n|grep '^[A-Z][A-Z0-9]*$')
		complete -c kill -o $i -d Send\ $i\ signal
		complete -c kill -o s -x -a $i\tSend\ $i\ signal -d "Send specified signal"
	end
end

complete -c kill -xa '(__fish_complete_pids)'
complete -c kill -s l -d "List names of available signals"

