function __fish_print_xrandr_modes --description 'Print xrandr modes'
	set -l output
	xrandr | string match -v -r '^(Screen|\s{4,})' | while read line
		switch $line
		case '  *'
			string trim $line | string replace -r '^(\S+)\s+(.*)$' "\$1\t\$2 [$output]"
		case '*'
			set output (string match -r '^\S+' $line)
		end
	end
end
