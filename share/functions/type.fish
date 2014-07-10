
function type --description "Print the type of a command"

	# Initialize
	set -l res 1
	set -l mode normal
	set -l selection all

	# Parse options
	set -l names
	if test (count $argv) -gt 0
		for i in (seq (count $argv))
			switch $argv[$i]
				case -t --type
					set mode type

				case -p --path
					set mode path

				case -P --force-path
					set mode path
					set selection files

				case -a --all
					set selection multi

				case -f --no-functions
					set selection files

				case -h --help
					__fish_print_help type
					return 0

				case --
					set names $argv[$i..-1]
					set -e names[1]
					break

				case '*'
					set names $argv[$i..-1]
					break
			end
		end
	end

	# Check all possible types for the remaining arguments
	for i in $names
		# Found will be set to 1 if a match is found
		set -l found 0

		if test $selection != files

			if contains -- $i (functions -na)
				set res 0
				set found 1
				switch $mode
					case normal
						printf (_ '%s is a function with definition\n') $i
						functions $i

					case type
						echo (_ 'function')

					case path
						echo

				end
				if test $selection != multi
					continue
				end
			end

			if contains -- $i (builtin -n)

				set res 0
				set found 1
				switch $mode
					case normal
						printf (_ '%s is a builtin\n') $i

					case type
						echo (_ 'builtin')

					case path
						echo
				end
				if test $selection != multi
					continue
				end
			end

		end

		set -l paths
		if test $selection != multi
			set paths (command -p $i)
		else
			set paths (which -a $i ^/dev/null)
		end
		for path in $paths
			set res 0
			set found 1
			switch $mode
				case normal
					printf (_ '%s is %s\n') $i $path

				case type
					echo (_ 'file')

				case path
					echo $path
			end
			if test $selection != multi
				continue
			end
		end

		if test $found = 0
			printf (_ "%s: Could not find '%s'\n") type $i >&2
		end

	end

	return $res
end

