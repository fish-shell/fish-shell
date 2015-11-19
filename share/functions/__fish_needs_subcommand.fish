function __fish_needs_subcommand --description "Completion helper to see if a command needs a subcommand or already has one, while skipping specified options"
	set -l flag_opts # Options that just cause some behavior to be switched - we just skip'em
	set -l exit_opts # Options that cause the command to exit regardless of subcommand
	set -l arg_opts # Options that take an argument - we assume that these have an option=value version that we skip
	# And the short versions of those
	set -l flag_shortopts
	set -l exit_shortopts
	set -l arg_shortopts # We also assume that the last short option in a group can have a =value
	set -l dest flag_opts
	for a in $argv
		switch $a
			case "--exit"
				set dest exit_opts
			case "--arg"
				set dest arg_opts
			case "--flag"
				set dest flag_opts
			case "--shortexit"
				set dest exit_shortopts
			case "--shortarg"
				set dest arg_shortopts
			case "--shortflag"
				set dest flag_shortopts
			case "*"
				set $dest $$dest $a
		end
	end
	set cmd (commandline -opc)
	if [ (count $cmd) -eq 1 ]
		return 0
	else
		set -l skip_next 1
		# Skip first word because it's the main command
		for c in $cmd[2..-1]
			test $skip_next -eq 0; and set skip_next 1; and continue
			switch $c
				# General options that can still take a command
				case --$flag_opts --$arg_opts"=*" -$flag_shortopts "-*"$arg_shortopts"="
					continue
					# General options with an argument we need to skip. The option=value versions have already been handled above
				case --$arg_opts "-*"$arg_shortopts"="
					set skip_next 0
					continue
					# General options that cause git to do something and exit - these behave like commands and everything after them is ignored
				case --$exit_opts -$exit_shortopts
					return 1
					# We assume that any other token that's not an argument to a general option is a command
				case "*"
					return 1
			end
		end
		return 0
	end
	return 1
end
