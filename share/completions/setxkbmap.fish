
set -l filter '"s/\S+\s\S+\s(.+)\((.+)\)/\1\t\2/;"'

complete -c setxkbmap -o '?' -o help -d 'Print this message'
complete -c setxkbmap -o compat    -d 'Specifies compatibility map component name' -xa "(cat /usr/share/X11/xkb/compat.dir | sed -r $filter)"
complete -c setxkbmap -o config    -d 'Specifies configuration file to use' -r
complete -c setxkbmap -o device    -d 'Specifies the device ID to use' -x
complete -c setxkbmap -o display   -d 'Specifies display to use' -x
complete -c setxkbmap -o geometry  -d 'Specifies geometry component name' -xa "(cat /usr/share/X11/xkb/geometry.dir | sed -r $filter)"
complete -c setxkbmap -o I         -d 'Add <dir> to list of directories to be used' -xa '(__fish_complete_directories)'
complete -c setxkbmap -o keycodes  -d 'Specifies keycodes component name' -xa "(cat /usr/share/X11/xkb/keycodes.dir | sed -r $filter)"
complete -c setxkbmap -o keymap    -d 'Specifies name of keymap to load' -xa "(cat /usr/share/X11/xkb/keymap.dir | sed -r $filter)"
complete -c setxkbmap -o layout    -d 'Specifies layout used to choose component names' -xa "(__fish_complete_setxkbmap layout)"
complete -c setxkbmap -o model     -d 'Specifies model used to choose component names' -xa "(__fish_complete_setxkbmap model)"
complete -c setxkbmap -o option    -d 'Adds an option used to choose component names' -xa "(__fish_complete_list , '__fish_complete_setxkbmap option')"
complete -c setxkbmap -o print     -d 'Print a complete xkb_keymap description and exit' 
complete -c setxkbmap -o query     -d 'Print the current layout settings and exit'
complete -c setxkbmap -o rules     -d 'Name of rules file to use' -x
complete -c setxkbmap -o symbols   -d 'Specifies symbols component name' -xa "(cat /usr/share/X11/xkb/symbols.dir | sed -r $filter)"
complete -c setxkbmap -o synch     -d 'Synchronize request w/X server'
complete -c setxkbmap -o types     -d 'Specifies types component name' -xa "(cat /usr/share/X11/xkb/types.dir | sed -r $filter)"
complete -c setxkbmap -o v -o verbose -d 'Sets verbosity (1..10).  Higher values yield more messages' -xa '(seq 1 10)'
complete -c setxkbmap -o variant   -d 'Specifies layout variant used to choose component names' -xa "(__fish_complete_setxkbmap variant)"

