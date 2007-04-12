
function alias --description "Legacy function for creating shellscript functions using an alias-like syntax"

	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help alias
				return 0
		end
	end

	set -l name
	set -l body
	switch (count $argv)

		case 1
			set -l tmp (echo $argv|sed -e "s/\([^=]\)=/\1\n/")
			set name $tmp[1]
			set body $tmp[2]

		case 2
			set name $argv[1]
			set body $argv[2]

		case \*
			printf ( _ "%s: Expected one or two arguments, got %d\n") alias (count $argv)
			return 1
	end

	eval "function $name; $body \$argv; end"
end
