function __fish_print_lpr_printers --description 'Print lpr printers'
	lpstat -p ^ /dev/null | sed 's/^\S*\s\(\S*\)\s\(.*\)$/\1\t\2/'


end
