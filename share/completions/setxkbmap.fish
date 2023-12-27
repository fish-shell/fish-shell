function __fish_complete_setxkbmap --description 'Complete setxkb options' --argument-names what
    sed -e "1,/! $what/d" -e '/^[[:space:]]*$/,$d' /usr/share/X11/xkb/rules/xorg.lst | sed -r 's/[[:space:]]+([^[:space:]]+)[[:space:]]+(.+)/\1\t\2/'
end

set -l filter '"s/[^[:space:]]+[[:space:]][^[:space:]]+[[:space:]](.+)\((.+)\)/\1\t\2/;"'

complete -c setxkbmap -o '?' -o help -d 'Print this message'
complete -c setxkbmap -o compat -d 'Specifies compatibility map component name' -xa "(sed -r $filter /usr/share/X11/xkb/compat.dir)"
complete -c setxkbmap -o config -d 'Specifies configuration file to use' -r
complete -c setxkbmap -o device -d 'Specifies the device ID to use' -x
complete -c setxkbmap -o display -d 'Specifies display to use' -x
complete -c setxkbmap -o geometry -d 'Specifies geometry component name' -xa "(sed -r $filter /usr/share/X11/xkb/geometry.dir)"
complete -c setxkbmap -o I -d 'Add <dir> to list of directories to be used' -xa '(__fish_complete_directories)'
complete -c setxkbmap -o keycodes -d 'Specifies keycodes component name' -xa "(sed -r $filter /usr/share/X11/xkb/keycodes.dir)"
complete -c setxkbmap -o keymap -d 'Specifies name of keymap to load' -xa "(sed -r $filter /usr/share/X11/xkb/keymap.dir)"
complete -c setxkbmap -o layout -d 'Specifies layout used to choose component names' -xa "(__fish_complete_setxkbmap layout)"
complete -c setxkbmap -o model -d 'Specifies model used to choose component names' -xa "(__fish_complete_setxkbmap model)"
complete -c setxkbmap -o option -d 'Adds an option used to choose component names' -xa "(__fish_complete_list , '__fish_complete_setxkbmap option')"
complete -c setxkbmap -o print -d 'Print a complete xkb_keymap description and exit'
complete -c setxkbmap -o query -d 'Print the current layout settings and exit'
complete -c setxkbmap -o rules -d 'Name of rules file to use' -x
complete -c setxkbmap -o symbols -d 'Specifies symbols component name' -xa "(sed -r $filter /usr/share/X11/xkb/symbols.dir)"
complete -c setxkbmap -o synch -d 'Synchronize request w/X server'
complete -c setxkbmap -o types -d 'Specifies types component name' -xa "(sed -r $filter /usr/share/X11/xkb/types.dir)"
complete -c setxkbmap -o v -o verbose -d 'Sets verbosity (1..10).  Higher values yield more messages' -xa '(seq 1 10)'
complete -c setxkbmap -o variant -d 'Specifies layout variant used to choose component names' -xa "(__fish_complete_setxkbmap variant)"
