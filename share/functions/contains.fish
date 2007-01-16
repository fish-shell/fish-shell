
function contains --description "Test if a key is contained in a set of values"
	while count $argv >/dev/null
		switch $argv[1]
			case '-h' '--h' '--he' '--hel' '--help'
				__fish_print_help contains
				return

			case '--'
				# End the loop, the next argument is the key
				set -e argv[1]
				break
			
			case '-*'
				printf (_ "%s: Unknown option '%s'\n") contains $argv[1]
				__fish_print_help contains >&2
				return 1

			case '*'
				# End the loop, we found the key
				break				

		end
		set -e argv[1]
	end

	if not count $argv >/dev/null
		printf (_ "%s: Key not specified\n") contains
		return 1
	end

	set key $argv[1]
	set -e argv[1]	

	#
	# Loop through values
	#

	printf "%s\n" $argv|sgrep -Fx -- $key >/dev/null
	return $status
end


