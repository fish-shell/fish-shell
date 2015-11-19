
function math --description "Perform math calculations in bc"
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help math
				return 0
		end

		# Stitch lines together manually
		# we can't rely on BC_LINE_LENGTH because some systems don't have a bc version "new" enough
		set -l out (echo $argv | bc | string replace -r '\\\\$' '' | string join '')
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
