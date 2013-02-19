
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
	set -l prefix
	switch (count $argv)

		case 0
 			echo "Fish implements aliases using functions. Use 'functions' builtin to see list of functions and 'functions function_name' to see function definition, type 'help alias' for more information."
			return 1
		case 1
			# Some seds (e.g. on Mac OS X), don't support \n in the RHS
			# Use a literal newline instead
			# http://sed.sourceforge.net/sedfaq4.html#s4.1
			set -l tmp (echo $argv|sed -e "s/\([^=]\)=/\1\\
/")
			set name $tmp[1]
			set body $tmp[2]

		case 2
			set name $argv[1]
			set body $argv[2]

		case \*
			printf ( _ "%s: Expected one or two arguments, got %d\n") alias (count $argv)
			return 1
	end


	# Prevent the alias from immediately running into an infinite recursion if
	# $body starts with the same command as $name.

	switch $body
		case $name $name\ \* $name\t\*
			if contains $name (builtin --names)
				set prefix builtin
			else
				set prefix command
			end
	end

	eval "function $name; $prefix $body \$argv; end"
end
