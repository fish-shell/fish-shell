function down-or-search
	# If we are already in search mode, continue
	if commandline --search-mode
		commandline -f history-search-forward
		return
	end

	# We are not in search mode.
	# If we are on the bottom line, start search mode, 
	# otherwise move down
	set lineno (commandline -L)
	set line_count (commandline|wc -l)

	echo on line $lineno of $line_count >&2

	switch $lineno
		case $line_count
		commandline -f history-search-forward
		
		case '*'
		commandline -f down-line
	end
end
