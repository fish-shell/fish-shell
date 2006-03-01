

function psub -d (N_ "Read from stdin into a file and output the filename. Remove the file when the command that called psub exits.")

	set -l filename
	set -l funcname

	if count $argv >/dev/null
		switch $argv[1]
			case '-h*' --h --he --hel --help

				help psub
				return 0

			case '*'
				printf (_ "%s: Unknown argument '%s'\n") psub $argv[1]
				return 1
		end
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

	# Write output to pipe. This needs to be done in the background so
	# that the command substitution exits without needing to wait for
	# all the commands to exit
	mkfifo $filename 
	cat >$filename &

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
	eval function $funcname --on-job-exit caller\; rm $filename\; functions -e $funcname\; end	

end
