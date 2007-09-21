function up-or-search
	# If we are already in search mode, continue
	if commandline --search-mode
		commandline -f history-search-backward
		return
	end

	set lineno (commandline -L)

	switch $lineno
		case 1
		commandline -f history-search-backward
		
		case '*'
		commandline -f up-line
	end
end
