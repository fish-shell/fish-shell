
complete -c xprop -o help -d (N_ "Display help and exit")
complete -c xprop -o grammar -d (N_ "Display grammar and exit")
complete -c xprop -o id -x -d (N_ "Select window by id")
complete -c xprop -o name -d (N_ "Select window by name")
complete -c xprop -o font -x -d (N_ "Display font properties")
complete -c xprop -o root -d (N_ "Select root window")
complete -c xprop -o display -d (N_ "Specify X server")
complete -c xprop -o len -x -d (N_ "Maximum display length")
complete -c xprop -o notype -d (N_ "Do not show property type")
complete -c xprop -o fs -r -d (N_ "Set format file")
complete -c xprop -o frame -d (N_ "Select a window by clicking on its frame" )
complete -c xprop -o remove -d (N_ "Remove property") -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

complete -c xprop -o set -d (N_ "Set property") -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

complete -c xprop -o spy -d (N_ "Examine property updates forever")
complete -c xprop -o f -d (N_ "Set format")
complete -c xprop -d Property -x -a "
(
	xprop -root -notype|cut -d ' ' -f 1|cut -d \t -f 1
)
"

