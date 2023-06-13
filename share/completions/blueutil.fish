complete -c blueutil -f

set -l commands \
    '-p --power -d --discoverable --favourites --favorites --inquiry --paired --recent --connected --info --is-connected --connect --disconnect --pair --unpair --add-favourite --add-favorite --remove-favourite --remove-favorite --format --wait-connect --wait-disconnect --wait-rssi -h --help -v --version'

complete -c blueutil -n "not __fish_seen_subcommand_from $commands" -a \
    '--power --discoverable --favorites --inquiry --paired --recent --connected --info --is-connected --connect --disconnect --pair --unpair --add-favorite --remove-favorite --format --wait-connect --wait-disconnect --wait-rssi --help --version'

complete -c blueutil -n '__fish_seen_subcommand_from -p --power -d --discoverable' -a 'on off toggle'
