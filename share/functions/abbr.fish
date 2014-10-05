function abbr --description "Manage abbreviations"
	if test (count $argv) -lt 1 -o (count $argv) -gt 2
		printf ( _ "%s: Expected one or two arguments, got %d\n") abbr (count $argv)
		__fish_print_help abbr
		return 1
	end

	switch $argv[1]
		case '-a' '--add'
			if __fish_abbr_get_by_key "$argv[2]" >/dev/null
				printf ( _ "%s: abbreviation %s already exists\n" ) abbr (__fish_abbr_print_key "$argv[2]" )
				return 2
			end
			set -U fish_user_abbreviations $fish_user_abbreviations "$argv[2]"
			return 0

		case '-r' '--remove'
			set -l index (__fish_abbr_get_by_key "$argv[2]")
			if test $index -gt 0
				set -e fish_user_abbreviations[$index]
				return 0
			else
				printf ( _ "%s: no such abbreviation %s\n" ) abbr (__fish_abbr_print_key "$argv[2]" )
				return 3
			end

		case '-s' '--show'
			for i in $fish_user_abbreviations
				echo abbr -a \'$i\'
			end
			return 0

		case '-l' '--list'
			for i in $fish_user_abbreviations
				__fish_abbr_print_key $i
			end
			return 0

		case '-h' '--help'
			__fish_print_help abbr
			return 0

		case '' '*'
			printf (_ "%s: Unknown option %s\n" ) abbr $argv[1]
			__fish_print_help abbr
			return 1
	end
end

function __fish_abbr_printable
	echo (__fish_abbr_print_key $argv)'="'(echo $argv | cut -f 2 -d =)'"'
end

function __fish_abbr_get_by_key
	for i in (seq (count $fish_user_abbreviations))
		switch $fish_user_abbreviations[$i]
			case (__fish_abbr_print_key $argv)'=*'
				echo $i
				return 0
		end
	end
	echo 0
	return 1
end

function __fish_abbr_print_key
	echo $argv| cut -f 1 -d =
end
