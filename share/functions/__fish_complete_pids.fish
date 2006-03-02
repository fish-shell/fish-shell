function __fish_complete_pids -d "Print a list of process identifiers along with brief descriptions"
	# This may be a bit slower, but it's nice - having the tty displayed is really handy
	ps --no-heading -o pid,comm,tty --ppid %self -N | sed -r 's/ *([0-9]+) +([^ ].*[^ ]|[^ ]) +([^ ]+)$/\1'\t'\2 [\3]/' ^/dev/null

	# If the above is too slow, this is faster but a little less useful
	#	pgrep -l -v -P %self | sed 's/ /'\t'/'
end

