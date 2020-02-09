function __fish_print_xwindows --description 'Print X windows'
    xwininfo -root -children | sed '/^\s\+0x/!d; /(has no name)/d; s/^\s*\(\S\+\)\s\+"\(.\+\)":\s\+(\(.*\)).*$/\1\t\2 (\3)/'
end
