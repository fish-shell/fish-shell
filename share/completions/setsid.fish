# setsid is a tool to run a program in a new session.
# It is part of the util-linux package.
# See: https://www.kernel.org/pub/linux/utils/util-linux

complete -c setsid -x -d 'Command to run' -a '(__fish_complete_subcommand)'
complete -c setsid -n __fish_no_arguments -l ctty -s c -d 'Set controlling terminal to current one'
complete -c setsid -n __fish_no_arguments -l wait -s w -d 'Wait until program ends and return its exit value'
complete -c setsid -n __fish_no_arguments -l version -s V -d 'Display version and exit'
complete -c setsid -n __fish_no_arguments -l help -s h -d 'Display help and exit'
