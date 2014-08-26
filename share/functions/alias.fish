
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
        set -l first_word
	switch (count $argv)

		case 0
 			echo "Fish implements aliases using functions. Use 'functions' builtin to see list of functions and 'functions function_name' to see function definition, type 'help alias' for more information."
			return 1
		case 1
			# Some seds (e.g. on Mac OS X), don't support \n in the RHS
			# Use a literal newline instead
			# http://sed.sourceforge.net/sedfaq4.html#s4.1
                        # The extra '' at the end is so $tmp[2] is guaranteed to work
                        set -l tmp (echo $argv|sed -e 's/\([^=]\{0,1\}\)=/\1\
/') ''
			set name $tmp[1]
			set body $tmp[2]

		case 2
			set name $argv[1]
			set body $argv[2]

		case \*
			printf ( _ "%s: Expected one or two arguments, got %d\n") alias (count $argv)
			return 1
	end

        # sanity check
        if test -z "$name"
                printf ( _ "%s: Name cannot be empty\n") alias
                return 1
        else if test -z "$body"
                printf ( _ "%s: Body cannot be empty\n") alias
                return 1
        end

        # Extract the first command from the body
        switch $body
                case \*\ \* \*\t\*
                        # note: strip leading spaces if present
                        set first_word (echo $body|sed -e 's/^[[:space:]]\{1,\}//;s/[[:space:]].*//;q')
                case '*'
                        set first_word $body
        end

	# Prevent the alias from immediately running into an infinite recursion if
	# $body starts with the same command as $name.

        if test $first_word = $name
                if contains $name (builtin --names)
                        set prefix builtin
                else
                        set prefix command
                end
	end

        eval "function $name --wraps $first_word; $prefix $body \$argv; end"
end
