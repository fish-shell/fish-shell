
function math --description "Perform math calculations in bc"
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help math
				return 0
		end

		# Override the locale so that the output can be used as input
		set -l LC_NUMERIC C
		# GNU bc extension
		set -l BC_LINE_LENGTH 0

		set -l out (printf '%g' (echo $argv| bc -l))
		test -z "$out"; and return 1
		echo $out
		switch $out
			case 0
				return 1
		end
		return 0
	end
	return 2

end

