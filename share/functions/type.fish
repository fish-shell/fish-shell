
function type --description "Print the type of a command"

	# Initialize
	set -l res 1
	set -l mode normal
	set -l multi no
	set -l selection all
	set -l IFS \n\ \t

	# Parse options
	set -l names
	if test (count $argv) -gt 0
		for i in (seq (count $argv))
			set -l arg $argv[$i]
			set -l needbreak 0
			while test -n $arg
				set -l flag $arg
				set arg ''
				switch $flag
					case '--*'
						# do nothing; this just prevents it matching the next case
					case '-??*'
						# combined flags
						set -l IFS
						echo -n $flag | read __ flag arg
						set flag -$flag
						set arg -$arg
				end
				switch $flag
					case -t --type
						if test $mode != quiet
							set mode type
						end

					case -p --path
						if test $mode != quiet
							set mode path
						end

					case -P --force-path
						if test $mode != quiet
							set mode path
						end
						set selection files

					case -a --all
						set multi yes

					case -f --no-functions
						set selection files

					case -q --quiet
						set mode quiet

					case -h --help
						__fish_print_help type
						return 0

					case --
						set names $argv[$i..-1]
						set -e names[1]
						set needbreak 1
						break

					case '*'
						set names $argv[$i..-1]
						set needbreak 1
						break
				end
			end
			if test $needbreak -eq 1
				break
			end
		end
	end

	# Check all possible types for the remaining arguments
	for i in $names
		# Found will be set to 1 if a match is found
		set -l found 0

		if test $selection != files

			if functions -q -- $i
				set res 0
				set found 1
				switch $mode
					case normal
						printf (_ '%s is a function with definition\n') $i
						if isatty stdout
							functions $i | fish_indent --ansi
						else
							functions $i | fish_indent
						end
					case type
						echo (_ 'function')
				end
				if test $multi != yes
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
				end
				if test $multi != yes
					continue
				end
			end

		end

		set -l paths
		if test $multi != yes
			set paths (command -s -- $i)
		else
			set paths (command which -a -- $i ^/dev/null)
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
			if test $multi != yes
				continue
			end
		end

		if begin; test $found = 0; and test $mode != quiet; end
			printf (_ "%s: Could not find '%s'\n") type $i >&2
		end

	end

	return $res
end
