if type -q -f sysctl
	# Only GNU and BSD sysctl seem to know "-h", so others should exit non-zero
	if sysctl -h >/dev/null ^/dev/null
		# Print sysctl keys and values, separated by a tab
		function __fish_sysctl_values
			sysctl -a ^/dev/null | string replace -a " = " "\t"
		end

		complete -c sysctl -a '(__fish_sysctl_values)' -f

		complete -c sysctl -s w --description 'parameter to use.'
		complete -c sysctl -s n -l values --description 'Only print values'
		complete -c sysctl -s e -l ignore --description 'Ignore errors about unknown keys'
		complete -c sysctl -s N -l names --description 'Only print names'
		complete -c sysctl -s q -l quiet --description 'Be quiet when setting values'
		complete -c sysctl -l write --description 'Write value'
		complete -c sysctl -o 'p[FILE]' -l 'load[' --description 'Load in sysctl settings from the file specified or /etc/sysctl'
		complete -c sysctl -s a -l all --description 'Display all values currently available'
		complete -c sysctl -l deprecated --description 'Include deprecated parameters too'
		complete -c sysctl -s b -l binary --description 'Print value without new line'
		complete -c sysctl -l system --description 'Load settings from all system configuration files'
		complete -c sysctl -s r -l pattern --description 'Only apply settings that match pattern'
		# Don't include these as they don't do anything
		# complete -c sysctl -s A --description 'Alias of -a'
		# complete -c sysctl -s d --description 'Alias of -h'
		# complete -c sysctl -s f --description 'Alias of -p'
		# complete -c sysctl -s X --description 'Alias of -a'
		# complete -c sysctl -s o --description 'Does nothing, exists for BSD compatibility'
		# complete -c sysctl -s x --description 'Does nothing, exists for BSD compatibility'
		complete -c sysctl -s h -l help --description 'Display help text and exit.'
		complete -c sysctl -s V -l version --description 'Display version information and exit.'
	else
		# OSX sysctl
		function __fish_sysctl_values
			sysctl -a ^/dev/null | string replace -a ":" "\t"
		end

		complete -c sysctl -a '(__fish_sysctl_values)' -f
		complete -c sysctl -s a --description 'Display all non-opaque values currently available'
		complete -c sysctl -s A --description 'Display all MIB variables'
		complete -c sysctl -s b --description 'Output values in a binary format'
		complete -c sysctl -s n --description 'Show only values, not names'
		complete -c sysctl -s w --description 'Set values'
		complete -c sysctl -s X --description 'Like -A, but prints a hex dump'
	end
end
