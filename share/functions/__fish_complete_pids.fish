function __fish_complete_pids -d "Print a list of process identifiers along with brief descriptions"
	# This may be a bit slower, but it's nice - having the tty displayed is really handy
	# 'tail -n +2' deletes the first line, which contains the headers
	# 'grep -v...' removes self from the output
	set -l SELF %self
	
	# Display the tty if available
	set -l sed_cmds 's/ *([0-9]+) +([^ ].*[^ ]|[^ ]) +([^ ]+) *$/\1'\t'\2 [\3]/'
	
	# But not if it's just question marks, meaning no tty
	set -l sed_cmds $sed_cmds 's/ *\[\?*\] *$//'
	
	ps axc -o pid,ucomm,tty | grep -v '^\s*'$SELF'\s' | tail -n +2 | sed -E "-e "$sed_cmds  ^/dev/null
end

