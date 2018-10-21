function __fish_print_xrandr_outputs --description 'Print xrandr outputs'
	xrandr | string replace -r --filter '^(\S+)\s+(.*)$' '$1\t$2' | string match -v -e Screen
end
