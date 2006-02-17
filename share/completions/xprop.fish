
complete -c xprop -o help -d (_ "Display help and exit")
complete -c xprop -o grammar -d (_ "Display grammar and exit")
complete -c xprop -o id -x -d (_ "Select window by id")
complete -c xprop -o name -d (_ "Select window by name")
complete -c xprop -o font -x -d (_ "Display font properties")
complete -c xprop -o root -d (_ "Select root window")
complete -c xprop -o display -d (_ "Specify X server")
complete -c xprop -o len -x -d (_ "Maximum display length")
complete -c xprop -o notype -d (_ "Do not show property type")
complete -c xprop -o fs -r -d (_ "Set format file")
complete -c xprop -o frame -d (_ "Select a window by clicking on its frame" )
complete -c xprop -o remove -d (_ "Remove property") -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

complete -c xprop -o set -d (_ "Set property") -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

complete -c xprop -o spy -d (_ "Examine property updates forever")
complete -c xprop -o f -d (_ "Set format")
complete -c xprop -d Property -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

