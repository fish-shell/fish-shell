function psub --description "Read from stdin into a file and output the filename. Remove the file when the command that called psub exits."

	set -l filename
	set -l funcname
	set -l use_fifo 1

	while set -q argv[1]
			switch $argv[1]
					case -h --help
						 __fish_print_help psub
						return 0

					case -f --file
						 set use_fifo 0
					end
			set -e argv[1]
		end

	if not status --is-command-substitution
		echo psub: Not inside of command substitution >&2
		return 1
	end

	set -l TMPDIR $TMPDIR
	if test -z "$TMPDIR[1]"
		set TMPDIR /tmp
	end

	if test $use_fifo = 1
		# Write output to pipe. This needs to be done in the background so
		# that the command substitution exits without needing to wait for
		# all the commands to exit
		while not set filename (mktemp "$TMPDIR[1]"/.psub.XXXXXXXXXX); end
		rm $filename
		mkfifo $filename
		cat | sh -c "cat </dev/stdin > $filename &"
	else
		set filename (mktemp "$TMPDIR[1]"/.psub.XXXXXXXXXX)
		cat >$filename
	end

	# Write filename to stdout
	echo $filename

	# Find unique function name
	while true
		set funcname __fish_psub_(random);
		if not functions $funcname >/dev/null ^/dev/null
			break;
		end
	end

	# Make sure we erase file when caller exits
	function $funcname --on-job-exit caller --inherit-variable filename --inherit-variable funcname
		command rm $filename
		functions -e $funcname
	end

end
