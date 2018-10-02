function __fish_complete_env_subcommand
    argparse -s s/ignore-environment u/unset h-help v-version -- (commandline -opc) (commandline -ct) 2>/dev/null
    or return
    if set -q argv[1]
        complete -C"$argv"
        return 0
    end
    return 1
end


complete -c env -a "(__fish_complete_env_subcommand)" -d "Command"

complete -c env -n 'not __fish_complete_env_subcommand' -a "(set -n)=" -x -d "Redefine variable"
complete -c env -n 'not __fish_complete_env_subcommand' -s i -l ignore-environment -d "Start with an empty environment"
complete -c env -n 'not __fish_complete_env_subcommand' -s u -l unset -d "Remove variable from the environment" -x -a "(set -n)"
complete -c env -n 'not __fish_complete_env_subcommand' -l help -d "Display help and exit"
complete -c env -n 'not __fish_complete_env_subcommand' -l version -d "Display version and exit"
