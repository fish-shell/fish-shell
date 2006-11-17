
function __fish_print_help
	set -l help (nroff -man $__fish_datadir/man/$argv.1)
	set -l lines (count $help)
	echo
	printf "%s\n" $help| tail -n (expr $lines - 5 ) | head -n (expr $lines - 8)
end