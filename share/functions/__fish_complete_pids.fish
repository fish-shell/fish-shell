function __fish_complete_pids -d "Print a list of process identifiers along with brief descriptions"
	# This may be a bit slower, but it's nice - having the tty displayed is really handy
	# 'tail -n +2' deletes the first line, which contains the headers
	#  %self is removed from output by string match -r -v
	set -l SELF %self
	
	# Display the tty if available
	# But not if it's just question marks, meaning no tty
	ps axc -o pid,ucomm,tty | string match -r -v '^\s*'$SELF'\s' | tail -n +2 | string replace -r ' *([0-9]+) +([^ ].*[^ ]|[^ ]) +([^ ]+) *$' '$1\t$2 [$3]' | string replace -r ' *\[\?*\] *$' ''
end

