
complete -c env -a "(set -n)=" -x -d (N_ "Redefine variable")

complete -c env -a "(__fish_complete_subcommand -- -u --unset)" -d (N_ "Command")

complete -c env -s i -l ignore-environment -d (N_ "Start with an empty environment")
complete -c env -s u -l unset -d (N_ "Remove variable from the environment") -x -a "(set -n)"
complete -c env -l help -d (N_ "Display help and exit")
complete -c env -l version -d (N_ "Display version and exit")

