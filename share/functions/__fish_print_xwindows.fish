function __fish_print_xwindows --description 'Print X windows'
	xwininfo -root -children | grep '^\s\+0x' | sed '/(has no name)/d; s/^\s*\(\S\+\)\s\+"\(.\+\)":\s\+(\(.*\)).*$/\1\t\2 (\3)/'

end
