function __fish_xprop_list_properties
    # TODO search commandline for a target window ("-root" or "-name foo")
    xprop -root | string split -f1 '('
end

complete -c xprop -o help -d "Display help and exit"
complete -c xprop -o grammar -d "Display grammar and exit"
complete -c xprop -o id -x -d "Select window by id"
complete -c xprop -o name -d "Select window by name"
complete -c xprop -o font -x -d "Display font properties"
complete -c xprop -o root -d "Select root window"
complete -c xprop -o display -d "Specify X server"
complete -c xprop -o len -x -d "Maximum display length"
complete -c xprop -o notype -d "Do not show property type"
complete -c xprop -o fs -r -d "Set format file"
complete -c xprop -o frame -d "Select a window by clicking on its frame"
complete -c xprop -o remove -d "Remove property" -x -a "( __fish_xprop_list_properties)"
complete -c xprop -o set -d "Set property" -x -a " (__fish_xprop_list_properties)"
complete -c xprop -o spy -d "Examine property updates forever"
complete -c xprop -o f -d "Set format"
complete -c xprop -d Property -x -a "( __fish_xprop_list_properties)"
