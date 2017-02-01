function __fish_xprop_list_properties
    # TODO search commandline for a target window ("-root" or "-name foo")
    xprop -root | cut -d'(' -f 1
end

complete -c xprop -o help --description "Display help and exit"
complete -c xprop -o grammar --description "Display grammar and exit"
complete -c xprop -o id -x --description "Select window by id"
complete -c xprop -o name --description "Select window by name"
complete -c xprop -o font -x --description "Display font properties"
complete -c xprop -o root --description "Select root window"
complete -c xprop -o display --description "Specify X server"
complete -c xprop -o len -x --description "Maximum display length"
complete -c xprop -o notype --description "Do not show property type"
complete -c xprop -o fs -r --description "Set format file"
complete -c xprop -o frame --description "Select a window by clicking on its frame"
complete -c xprop -o remove --description "Remove property" -x -a "( __fish_xprop_list_properties)"
complete -c xprop -o set --description "Set property" -x -a " (__fish_xprop_list_properties)"
complete -c xprop -o spy --description "Examine property updates forever"
complete -c xprop -o f --description "Set format"
complete -c xprop -d Property -x -a "( __fish_xprop_list_properties)"

