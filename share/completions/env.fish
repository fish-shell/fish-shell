
complete -c env -a "(set -n)=" -x -d "Redefine variable"

complete -c env -a "(__fish_complete_subcommand -- -u --unset)" -d "Command"

complete -c env -s i -l ignore-environment -d "Start with an empty environment"
complete -c env -s u -l unset -d "Remove variable from the environment" -x -a "(set -n)"
complete -c env -l help -d "Display help and exit"
complete -c env -l version -d "Display version and exit"
