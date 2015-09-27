
function math --description "Perform math calculations in bc"
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help math
				return 0
		end

		set -lx BC_LINE_LENGTH 0
		set -l out (echo $argv | bc)
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
