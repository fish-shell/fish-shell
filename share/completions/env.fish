if env --version &>/dev/null
    set -l is_gnu --is-gnu
end

function __fish_env_remaining_args -V is_gnu
    if set -q is_gnu[1]
        argparse -s i/ignore-environment u/unset= help version -- (commandline -opc) (commandline -ct) 2>/dev/null
        or return 1
    else
        argparse -s 0 i P= S= u= v -- (commandline -opc) (commandline -ct) 2>/dev/null
        or return 1
    end

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
complete -c env -n 'not __fish_env_remaining_args; and not string match -eq = -- (commandline -ct)' -a "(set -n)=" -f -d "Redefine variable"

if set -q is_gnu
    # complete VAR= only if the cursor is left of the =, otherwise complete the file right of the =
    complete -c env -n 'not __fish_env_remaining_args' -s i -l ignore-environment -d "Start with an empty environment"
    complete -c env -n 'not __fish_env_remaining_args' -s u -l unset -d "Unset environment variable" -x -a "(set -n)"
    complete -c env -n 'not __fish_env_remaining_args' -l help -d "Display help and exit"
    complete -c env -n 'not __fish_env_remaining_args' -l version -d "Display version and exit"
else
    # complete VAR= only if the cursor is left of the =, otherwise complete the file right of the =
    complete -c env -n 'not __fish_env_remaining_args' -s 0 -d "End output lines with NUL"
    complete -c env -n 'not __fish_env_remaining_args' -s i -d "Start with empty environment"
    complete -c env -n 'not __fish_env_remaining_args' -s P -d "Provide an alternate PATH"
    complete -c env -n 'not __fish_env_remaining_args' -s S -d "Split argument into args on ' '"
    complete -c env -n 'not __fish_env_remaining_args' -s u -d "Unset environment variable" -x -a "(set -n)"
    complete -c env -n 'not __fish_env_remaining_args' -s v -d "Verbose output on processing"
end
