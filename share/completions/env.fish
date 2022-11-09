set -l is_gnu
if env --version &>/dev/null
    set is_gnu --is-gnu
end

# Returns 0 if we're after `env` and all previous tokens have an equal sign
function __fish_env_defining_vars
    not string match -ev -- = (commandline -o)[2..-2] | string match -rq .
end

# Returns 0 if we're after `env` and all previous tokens have not yet contained an equal sign
function __fish_env_not_yet_vars
    not string match -qe = (commandline)
end

# Generate a list of possible variable names to redefine, excluding any already redefined.
function __fish_env_redefine_vars
    set -l vars (set --names -x)

    set cmdline "$(commandline -o)"
    for var in $vars
        if not string match -e -- $var= $cmdline
            echo $var=
        end
    end
end

# Get the text after all env arguments and variables, so we can complete it as a regular command
function __fish_env_remaining_args -V is_gnu
    set -l argv (commandline -opc) (commandline -ct)
    if set -q is_gnu[1]
        argparse -s i/ignore-environment u/unset= help version -- $argv 2>/dev/null
        or return 0
    else
        argparse -s 0 i P= S= u= v -- $argv 2>/dev/null
        or return 0
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
    test -n "$argv[1]"
end

# Generate a completion for the executable to execute under `env`
function __fish_complete_env_subcommand
    if set -l argv (__fish_env_remaining_args)
        __fish_complete_subcommand --commandline $argv
    end
end

complete -c env -a "(__fish_complete_env_subcommand)"
# Complete the name of the variable to redefine
complete -c env -n '__fish_env_defining_vars; and not string match -eq = -- (commandline -ct)' -a "(__fish_env_redefine_vars)" -f -d "Redefine variable"

if set -q is_gnu
    complete -c env -n '__fish_env_not_yet_vars' -s i -l ignore-environment -d "Start with an empty environment"
    complete -c env -n '__fish_env_not_yet_vars' -s u -l unset -d "Unset environment variable" -x -a "(set --names -x)"
    complete -c env -n '__fish_env_not_yet_vars' -l help -d "Display help and exit"
    complete -c env -n '__fish_env_not_yet_vars' -l version -d "Display version and exit"
else
    complete -c env -n '__fish_env_not_yet_vars' -s 0 -d "End output lines with NUL"
    complete -c env -n '__fish_env_not_yet_vars' -s i -d "Start with empty environment"
    complete -c env -n '__fish_env_not_yet_vars' -s P -d "Provide an alternate PATH"
    complete -c env -n '__fish_env_not_yet_vars' -s S -d "Split argument into args on ' '"
    complete -c env -n '__fish_env_not_yet_vars' -s u -d "Unset environment variable" -x -a "(set --names -x)"
    complete -c env -n '__fish_env_not_yet_vars' -s v -d "Verbose output on processing"
end
