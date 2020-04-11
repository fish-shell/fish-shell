set -l cmds 'scontrols scontents controls contents sget sset cset cget set get'
complete -c amixer -xa "$cmds" -n "not __fish_seen_subcommand_from $cmds"
complete -c amixer -n '__fish_seen_subcommand_from sset sget get set' -xa "(amixer scontrols | string split -f 2 \')"

complete -c amixer -s h -l help -d 'this help'
complete -c amixer -s c -l card -r -d 'select the card'
complete -c amixer -s D -l device -r -d 'select the device, default \'default\''
complete -c amixer -s d -l debug -d 'debug mode'
complete -c amixer -s n -l nocheck -d 'do not perform range checking'
complete -c amixer -s v -l version -d 'print version of this program'
complete -c amixer -s q -l quiet -d 'be quiet'
complete -c amixer -s i -l inactive -d 'show also inactive controls'
complete -c amixer -s a -l abstract -d 'select abstraction level' -xa 'none basic'
complete -c amixer -s s -l stdin -d 'Read and execute commands from stdin sequentially'
