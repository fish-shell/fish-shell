__fish_make_completion_signals

for i in $__kill_signals
	set number (echo $i | cut -d " " -f 1)
	set name (echo $i | cut -d " " -f 2)
	complete -c kill -o $number -d $name
	complete -c kill -o $name
	complete -c kill -s s -x -a "$number $name"
end

complete -c kill -xa '(__fish_complete_pids)'

if kill -L > /dev/null ^ /dev/null
	complete -c kill -s s -l signal -d "Signal to send"
	complete -c kill -s l -l list -d "Printf list of signal names, or name of given SIG NUMBER"
	complete -c kill -s L -l table -d " Print signal names and their corresponding numbers"
	complete -c kill -s a -l all -d "Do not restrict the commandname-to-pid conversion to processes with the same uid as the present process"
	complete -c kill -s p -l pid -d "Only print pid of the named processes, do not send any signals"
	complete -c kill -s q -l queue -d "Use sigqueue(2) rather than kill(2).  The value argument is an integer that is sent along with the signal."
	complete -c kill -l verbos -d "Print pid(s) that will be signaled with kill along with the signal."
else # OS X
	complete -c kill -s s -d "Signal to send"
	complete -c kill -s l -d "Printf list of signal names, or name of given SIG NUMBER"
end