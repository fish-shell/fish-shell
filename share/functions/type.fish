
function type --description "Print the type of a command"

	# Initialize
	set -l res 1
	set -l mode normal
	set -l selection all
		
	#
	# Get options
	#
	set -l options
	set -l shortopt tpPafh
	if not getopt -T > /dev/null
		# GNU getopt
		set -l longopt type,path,force-path,all,no-functions,help
		set options -o $shortopt -l $longopt --
		# Verify options
		if not getopt -n type $options $argv >/dev/null
			return 1
		end
	else
		# Old getopt, used on OS X
		set options $shortopt
		# Verify options
		if not getopt $options $argv >/dev/null
			return 1
		end
	end

	# Do the real getopt invocation
	set -l tmp (getopt $options $argv)

	# Break tmp up into an array
	set -l opt
	eval set opt $tmp
	
	for i in $opt
		switch $i
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
				 break

		end
	end

	# Check all possible types for the remaining arguments
	for i in $argv

		switch $i
			case '-*'
				 continue
		end

		# Found will be set to 1 if a match is found
		set found 0

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
			set paths (which $i ^/dev/null)
		else
			set paths (which -a $i ^/dev/null)
		end
		for path in $paths
			if test -x (echo $path)
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
		end

		if test $found = 0
			printf (_ "%s: Could not find '%s'\n") type $i
		end

	end

	return $res
end

