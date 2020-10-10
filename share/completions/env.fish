function __fish_env_remaining_args
    argparse -s s/ignore-environment u/unset= h-help v-version -- (commandline -opc) (commandline -ct) 2>/dev/null
    or return 1

    # argv[1] is `env` or an alias.
    set -e argv[1]

    # Remove all VAR=VAL arguments up to the first that isn't
    while set -q argv[1]
        if string match -q '*=*' -- $argv[1]
            or string match -q -- '-*' $argv[1]
            set -e argv[1]
        else
            break
        end
    end

    string join \n -- $argv

    # Return true if there is a subcommand.
    set -q argv[1]
end

function __fish_complete_env_subcommand
    if set -l argv (__fish_env_remaining_args)
        __fish_complete_subcommand --commandline $argv
    end
end

complete -c env -a "(__fish_complete_env_subcommand)"

# complete VAR= only if the cursor is left of the =, otherwise complete the file right of the =
complete -c env -n 'not __fish_env_remaining_args; and not string match -eq = -- (commandline -ct)' -a "(set -n)=" -f -d "Redefine variable"
complete -c env -n 'not __fish_env_remaining_args' -s i -l ignore-environment -d "Start with an empty environment"
complete -c env -n 'not __fish_env_remaining_args' -s u -l unset -d "Remove variable from the environment" -x -a "(set -n)"
complete -c env -n 'not __fish_env_remaining_args' -l help -d "Display help and exit"
complete -c env -n 'not __fish_env_remaining_args' -l version -d "Display version and exit"
