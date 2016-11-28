function __fish_print_xrandr_outputs --description 'Print xrandr outputs'
    xrandr | sed '/^Screen\|^ /d; s/^\(\S\+\) \+\(.\+\)/\1\t\2/'


end
