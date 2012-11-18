__fish_make_completion_signals

for i in $__kill_signals
	set number (echo $i | cut -d " " -f 1)
	set name (echo $i | cut -d " " -f 2)
	complete -c killall -o $number -d $name
	complete -c killall -o $name -d $name
	complete -c killall -o s -x -a \"$number\tSend\ $name\ signal\"
	complete -c killall -o s -x -a \"$name\tSend\ $name\ signal\"
end

complete -c killall -xa '(__fish_complete_proc)'
complete -c killall -s e -l exact -d 'Require an exact match for very long names'
complete -c killall -s I -l ignore-case -d 'Do case insensitive process name match'
complete -c killall -s g -l process-group -d 'Kill the process group to which the process belongs. The kill signal is only sent once per group, even if multiple processes belonging to the same process group were found'
complete -c killall -s i -l interactive -d 'Interactively ask for confirmation before killing'
complete -c killall -s u -l user -d 'Kill only processes the specified user owns. Command names are optional'
complete -c killall -s w -l wait -d 'Wait  for  all  killed processes to die. killall checks once per second if any of the killed processes still exist and only returns if none are left.  Note that killall may wait forever if the signal was ignored, had no effect, or if the process stays in zombie state'


