function __fish_print_commands --description "Print a list of documented fish commands"
	if test -d $__fish_datadir/man/man1/
		find $__fish_datadir/man/man1/ -type f -name \*.1 -execdir basename '{}' .1 ';'
	end
end
