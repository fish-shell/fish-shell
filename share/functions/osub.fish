
function psub --description "Read from stdin into a file and output the filename. Remove the file when the command that called psub exits."

	set -l dirname
	set -l filename
	set -l funcname
	set -l suffix
	set -l use_fifo 1

	while count $argv >/dev/null

		switch $argv[1]
			case -h --help
				__fish_print_help psub
				return 0

			case -f --file
				set use_fifo 0
				set -e argv[1]

			case -s --suffix
				if not set -q argv[2]
					printf "psub: missing operand\n"
					return 1
				end
				set suffix $argv[2]
				set -e argv[1..2]

			case --
				set -e argv[1]
				break

			case "-?" "--*"
				printf "psub: invalid option: '%s'\n" $argv[1]
				return 1

			case "-*"
				# Ungroup short options: -hfs => -h -f -s
				set opts "-"(string sub -s 2 -- $argv[1] | string split "")
				set -e argv[1]
				set argv $opts $argv

			case "*"
				printf "psub: extra operand: '%s'\n" $argv[1]
				return 1
		end
	end

	if not status --is-command-substitution
		echo psub: Not inside of command substitution >&2
		return 1
	end

	set -l TMPDIR $TMPDIR
	if test -z "$TMPDIR[1]"
		set TMPDIR /tmp
	end

	if test use_fifo = 1
		# Write output to pipe. This needs to be done in the background so
		# that the command substitution exits without needing to wait for
		# all the commands to exit
		set dirname (mktemp -d "$TMPDIR[1]"/.psub.XXXXXXXXXX); or return
		set filename $dirname/psub.fifo"$suffix"
		mkfifo $filename
		cat >$filename &
	else if test -z $suffix
		set filename (mktemp "$TMPDIR[1]"/.psub.XXXXXXXXXX)
		cat >$filename
	else
		set dirname (mktemp -d "$TMPDIR[1]"/.psub.XXXXXXXXXX)
		set filename $dirname/psub"$suffix"
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
	function $funcname --on-job-exit caller --inherit-variable filename --inherit-variable dirname --inherit-variable funcname
		command rm $filename
		if count $dirname >/dev/null
			command rmdir $dirname
		end
		functions -e $funcname
	end

end
