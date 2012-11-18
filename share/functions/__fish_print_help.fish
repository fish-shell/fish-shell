
function __fish_print_help --description "Print help message for the specified fish function or builtin" --argument item

	switch $argv[1]
		case '.'
		set item source

		case '*'
		set item $argv[1]
	end

	# Do nothing if the file does not exist
	if not test -e "$__fish_datadir/man/man1/$item.1"
		return
	end

	# These two expressions take care of underlines (Should be italic)
	set -l cmd1 s/_\x08'\(.\)'/(set_color --underline)\\1(set_color normal)/g
	set -l cmd2 s/'\(.\)'\x08_/(set_color --underline)\\1(set_color normal)/g

	# This expression should take care of bold characters. It's not
	# waterproof, since it doesn't check that the same character is
	# used both before and after the backspace, since regular
	# languages don't allow backreferences.
	set -l cmd3 s/.\x08'\(.\)'/(set_color --bold)\\1(set_color normal)/g

	# Combine all expressions in a single variable
	set -l sed_cmd -e $cmd1 -e $cmd2 -e $cmd3

	# Render help output, save output into the variable 'help'
	set -l help (nroff -man "$__fish_datadir/man/man1/$item.1" ^ /dev/null )
	set -l lines (count $help)

	# Print an empty line first
	echo

	# Filter and print help
	printf "%s\n" $help| tail -n (expr $lines - 5) | head -n (expr $lines - 8) | sed $sed_cmd

end
