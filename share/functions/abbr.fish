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
		# Bail out early if the exact abbr is already in
		# This depends on the separator staying the same, but that's the common case (config.fish)
		contains -- $mode_arg $fish_user_abbreviations; and return 0
		set -l key
		set -l value
		__fish_abbr_parse_entry $mode_arg key value
		# ensure the key contains at least one non-space character
		if not string match -qr "\w" -- $key
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
		set -l key
		__fish_abbr_parse_entry $mode_arg key
		if set -l idx (__fish_abbr_get_by_key $key)
			set -e fish_user_abbreviations[$idx]
			return 0
		else
			printf ( _ "%s: no such abbreviation '%s'\n" ) abbr $key >&2
			return 2
		end

	case 'show'
		for i in $fish_user_abbreviations
			__fish_abbr_parse_entry $i key value
			
			# Check to see if either key or value has a leading dash
			# If so, we need to write --
			set -l opt_double_dash ''
			switch $key ; case '-*'; set opt_double_dash ' --'; end
			switch $value ; case '-*'; set opt_double_dash ' --'; end
			echo abbr$opt_double_dash (string escape -- $key $value)
		end
		return 0

	case 'list'
		for i in $fish_user_abbreviations
			set -l key
			__fish_abbr_parse_entry $i key
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
	set -l count (count $fish_user_abbreviations)
	if test $count -gt 0
		set -l key $argv[1] # This assumes the key is valid and pre-parsed
		for i in (seq $count)
			set -l key_i
			__fish_abbr_parse_entry $fish_user_abbreviations[$i] key_i
			if test "$key" = "$key_i"
				echo $i
				return 0
			end
		end
	end
	return 1
end

function __fish_abbr_parse_entry -S -a __input __key __value
	if test -z "$__key"
		set __key __
	end
	if test -z "$__value"
		set __value __
	end
	# A "=" _before_ any space - we only read the first possible separator
	# because the key can contain neither spaces nor "="
	if string match -qr '^[^ ]+=' -- $__input
		# No need for bounds-checking because we already matched before
		set -l KV (string split "=" -m 1 -- $__input)
		set $__key $KV[1]
		set $__value $KV[2]
	else if string match -qr '^[^ ]+ .*' -- $__input
		set -l KV (string split " " -m 1 -- $__input)
		set $__key $KV[1]
		set $__value $KV[2]
	else
		# This is needed for `erase` et al, where we want to allow passing a value
		set $__key $__input
	end
	return 0
end
