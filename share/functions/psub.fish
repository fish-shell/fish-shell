

function psub --description "Read from stdin into a file and output the filename. Remove the file when the command that called psub exits."

	set -l filename
	set -l funcname
	set -l use_fifo 1
	set -l shortopt -o hf
	set -l longopt -l help,file

	if getopt -T >/dev/null
		set longopt 
	end

	if not getopt -n psub -Q $shortopt $longopt -- $argv
		return 1
	end

	set -l tmp (getopt $shortopt $longopt -- $argv)

	eval set opt $tmp

	while count $opt >/dev/null

		switch $opt[1]
			case -h --help
				__fish_print_help psub
				return 0

			case -f --file
				set use_fifo 0

			case --
				set -e opt[1]
				break

		end

		set -e opt[1]

	end

	if not status --is-command-substitution
		echo psub: Not inside of command substitution >&2
		return
	end

	# Find unique file name for writing output to
	while true
		set filename /tmp/.psub.(echo %self).(random);
		if not test -e $filename
			break;
		end
	end

	if test use_fifo = 1
		# Write output to pipe. This needs to be done in the background so
		# that the command substitution exits without needing to wait for
		# all the commands to exit
		mkfifo $filename 
		cat >$filename &
	else
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
	eval function $funcname --on-job-exit caller\; command rm $filename\; functions -e $funcname\; end	

end
