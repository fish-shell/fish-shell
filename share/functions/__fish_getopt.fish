function __fish_getopt
	set -l shortopt $argv[1]
	set -e argv[1]
	set -l gnuopt
	set -l args
	while count $argv >/dev/null
		if test $argv[1] = "--"
			set args $argv
			set -e args[1]
			break
		else
			set gnuopt $gnuopt $argv[1]
			set -e argv[1]
		end
	end
	set -l getopt
	if not getopt -T >/dev/null
		set getopt -o $shortopt $gnuopt -- $args
	else
		set getopt $shortopt $args
	end
	getopt $getopt
	return $status
end
