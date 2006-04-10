
function type -d (N_ "Print the type of a command")

	# Initialize
	set -l res 1
	set -l mode normal
	set -l selection all

	#
	# Get options
	#
	set -l shortopt -o tpPafh
	set -l longopt
	if not getopt -T >/dev/null
		set longopt -l type,path,force-path,all,no-functions,help
	end

	if not getopt -n type -Q $shortopt $longopt -- $argv
		return 1
	end

	set -l tmp (getopt $shortopt $longopt -- $argv)

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
				 help type
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
						printf (_ 'function\n')

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
						printf (_ 'builtin\n')

					case path
						echo
				end
				if test $selection != multi
					continue
				end
			end

		end

		set -l path (which $i ^/dev/null)
		if test -x (echo $path)
			set res 0
			set found 1
			switch $mode
				case normal
					printf (_ '%s is %s\n') $i $path

					case type
						printf (_ 'file\n')

					case path
						echo $path
			end
			if test $selection != multi
				continue
			end
		end

		if test $found = 0
			printf (_ "%s: Could not find '%s'\n") type $i
		end

	end

	return $res
end

