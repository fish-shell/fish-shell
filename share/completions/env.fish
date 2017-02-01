
complete -c env -a "(set -n)=" -x --description "Redefine variable"

complete -c env -a "(__fish_complete_subcommand -- -u --unset)" --description "Command"

complete -c env -s i -l ignore-environment --description "Start with an empty environment"
complete -c env -s u -l unset --description "Remove variable from the environment" -x -a "(set -n)"
complete -c env -l help --description "Display help and exit"
complete -c env -l version --description "Display version and exit"
