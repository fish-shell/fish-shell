#
# This file defines various shellscript functions. Most of them are
# meant to be used directly by the user, but some of them, typically
# the ones whose name start with '__fish_', are only meant to be used
# internally by fish.
#

function contains -d (N_ "Test if a key is contained in a set of values")
	while set -q argv
		switch $argv[1]
			case '-h' '--h' '--he' '--hel' '--help'
				help contains
				return

			case '--'
				# End the loop, the next argument is the key
				set -e argv[1]
				break
			
			case '-*'
				printf (_ "%s: Unknown option '%s'\n") contains $argv[$i]
				help contains
				return 1

			case '*'
				# End the loop, we found the key
				break				

		end
		set -e argv[1]
	end

	if not set -q argv 
		printf (_ "%s: Key not specified\n") contains
		return 1
	end

	set -- key $argv[1]
	set -e argv[1]	

	#
	# Loop through values
	#

	printf "%s\n" $argv|grep -Fx -- $key >/dev/null
	return $status
end


