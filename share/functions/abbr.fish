function abbr --description "Manage abbreviations"
	# parse arguments
	set -l mode
	set -l mode_flag # the flag that was specified, for better errors
	set -l mode_arg
	set -l needs_arg no
	while set -q argv[1]
		if test $needs_arg = single
			set mode_arg $argv[1]
			set needs_arg no
		else if test $needs_arg = coalesce
			set mode_arg "$argv"
			set needs_arg no
			set -e argv
		else
			set -l new_mode
			switch $argv[1]
			case '-h' '--help'
				__fish_print_help abbr
				return 0
			case '-a' '--add'
				set new_mode add
				set needs_arg coalesce
			case '-e' '--erase'
				set new_mode erase
				set needs_arg single
			case '-l' '--list'
				set new_mode list
			case '-s' '--show'
				set new_mode show
			case '--'
				set -e argv[1]
				break
			case '-*'
				printf ( _ "%s: invalid option -- %s\n" ) abbr $argv[1] >&2
				return 1
			case '*'
				break
			end
			if test -n "$mode" -a -n "$new_mode"
				# we're trying to set two different modes
				printf ( _ "%s: %s cannot be specified along with %s\n" ) abbr $argv[1] $mode_flag >&2
				return 1
			end
			set mode $new_mode
			set mode_flag $argv[1]
		end
		set -e argv[1]
	end
	if test $needs_arg != no
		printf ( _ "%s: option requires an argument -- %s\n" ) abbr $mode_flag >&2
		return 1
	end
	
	# If run with no options, treat it like --add if we have an argument, or
	# --show if we do not have an argument
	if test -z "$mode"
		if set -q argv[1]
			set mode 'add'
			set mode_arg "$argv"
			set -e argv
		else
			set mode 'show'
		end
	end

	# none of our modes want any excess arguments
	if set -q argv[1]
		printf ( _ "%s: Unexpected argument -- %s\n" ) abbr $argv[1] >&2
		return 1
	end

	switch $mode
	case 'add'
		# Convert from old "key=value" to new "key value" syntax
		if string match -qr '^[^ ]+=' -- $mode_arg
			set mode_arg (string replace "=" " " -- $mode_arg)
		end

		# Bail out early if the exact abbr is already in
		contains -- $mode_arg $fish_user_abbreviations; and return 0
		set -l key
		set -l value
		set -l kv (__fish_abbr_split $mode_arg)
		set key $kv[1]
		set value $kv[2]
		# ensure the key contains at least one non-space character
		if not string match -qr "[^\s]" -- $key
			printf ( _ "%s: abbreviation must have a non-empty key\n" ) abbr >&2
			return 1
		end
		if not string match -qr "\w" -- $value
			printf ( _ "%s: abbreviation must have a value\n" ) abbr >&2
			return 1
		end
		if set -l idx (__fish_abbr_get_by_key $key)
			# erase the existing abbreviation
			set -e fish_user_abbreviations[$idx]
		end
		if not set -q fish_user_abbreviations
			# initialize as a universal variable, so we can skip the -U later
			# and therefore work properly if someone sets this as a global variable
			set -U fish_user_abbreviations
		end
		set fish_user_abbreviations $fish_user_abbreviations $mode_arg
		return 0

	case 'erase'
		set -l key (__fish_abbr_split $mode_arg)[1]
		if set -l idx (__fish_abbr_get_by_key $key)
			set -e fish_user_abbreviations[$idx]
			return 0
		else
			printf ( _ "%s: no such abbreviation '%s'\n" ) abbr $key >&2
			return 2
		end

	case 'show'
		for i in $fish_user_abbreviations
			set -l kv (__fish_abbr_split $i)
			set -l key $kv[1]
			set -l value $kv[2]
			
			# Check to see if either key or value has a leading dash
			# If so, we need to write --
			string match -q -- '-*' $key $value; and set -l opt_double_dash '--'
			echo abbr $opt_double_dash (string escape -- $key $value)
		end
		return 0

	case 'list'
		for i in $fish_user_abbreviations
			set -l key (__fish_abbr_split $i)[1]
			printf "%s\n" $key
		end
		return 0
	end
end

function __fish_abbr_get_by_key
	if not set -q argv[1]
		echo "__fish_abbr_get_by_key: expected one argument, got none" >&2
		return 2
	end
	# Going through all entries is still quicker than calling `seq`
	set -l keys
	for kv in $fish_user_abbreviations
		# If this does not match, we have screwed up before and the error should be reported
		set keys $keys (string split " " -m 1 -- $kv)[1]
	end
	if set -l idx (contains -i -- $argv[1] $keys)
		echo $idx
		return 0
	end
	return 1
end

function __fish_abbr_split -a input
	# Because we always save space-separated, we can be certain that this will match
	string split " " -m 1 -- $input
end
