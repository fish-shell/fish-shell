function __fish_complete_setxkbmap --description 'Complete setxkb options' --argument-names what
    sed -e "1,/! $what/d" -e '/^\s*$/,$d' /usr/share/X11/xkb/rules/xorg.lst | sed -r 's/\s+(\S+)\s+(.+)/\1\t\2/'
end
