set -l is_gnu
if env --version &>/dev/null
    set is_gnu --is-gnu
end

# Returns 0 if we're after `env` and all previous tokens have an equal sign or were switches
function __fish_env_defining_vars
    set last ""
    for token in (commandline -cxp)[2..] # 2.. excludes `env`. -cx already ignores the variable being completed.
        # Is a switch, defined an env variable, or was a variable name to unset (after -u)
        string match -r -- '^-|=' $token || string match -rq -- '^(-u|--unset)$' "$last" || return 1
        set last $token
    end
end

# Returns 0 if we're after `env` and all previous tokens have not yet contained an equal sign
# Prevents `env` completions from completing payload completions.
function __fish_env_not_yet_vars
    not string match -qe -- = (commandline -c)
end

# Generate a list of possible variable names to redefine, excluding any already redefined.
function __fish_env_redefine_vars
    set -l vars (set --names -x)

    set cmdline "$(commandline -xp)"
    for var in $vars
        if not string match -eq -- $var= $cmdline
            echo $var=
        end
    end
end

# Generate a list of possible variable names to define from completion history
function __fish_env_names_from_history
    set -l token (commandline -ct)
    # Since this is always going to be a best-effort kind of thing, limit this to uppercased variables by convention.
    # This prevents us from having to parse quotes to figure out what was part of the payload and what wasn't.
    for var in (history search --prefix "env " | string match -ra '\b([A-Z0-9_]+)=' --groups-only)
        echo $var=
    end
end

# Generate a list of possible completions for the current variable name from history
function __fish_env_values_from_history
    string match -rq -- "(?<name>.+)=(?<value>.*)" (commandline -ct); or return 1

    # Caveat lector: very crude multi-word tokenization handling below!
    set -l rname (string escape --style=regex -- $name)
    set -l search (string trim -c \'\" "$value")
    set -l rsearch "$(string escape --style=regex -- $search)"
    # Search multi-word values with quotes
    set -l query '.*\b'$name'=([\'"])('$rsearch'.+?)\1.*'
    set -l matches (history search --prefix "env " | string replace -rfa $query '$2')
    # Search multi-word values without quotes
    set -l query '.*\b'$name'=('$rsearch'[^\'" ]+).*'
    set -a matches (history search --prefix "env " | string replace -rfa $query '$1')
    # Display results without quotes
    set matches (printf "%s\n" $matches | sort -u)
    printf "$name=%s\n" $matches
end

# Get the text after all env arguments and variables, so we can complete it as a regular command
function __fish_env_remaining_args -V is_gnu
    set -l argv (commandline -xpc | string escape) (commandline -ct)
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
        complete -C "$argv"
    end
end

# Complete the name of the variable from current definitions
complete -c env -n '__fish_env_defining_vars; and not string match -eq = -- (commandline -ct)' -a "(__fish_env_redefine_vars)" -f -d "Redefine variable"
# Complete the name of the variable from history
complete -c env -n '__fish_env_defining_vars; and not string match -eq = -- (commandline -ct)' -a "(__fish_env_names_from_history)" -f -d Historical
# Complete the value for FOO= from history
# TODO: NO_ESCAPE when that becomes available
complete -c env -n '__fish_env_defining_vars; and string match -eq = -- (commandline -ct)' -a "(__fish_env_values_from_history)" -f

# Complete normally after we are done with `env` stuff
complete -c env -fa "(__fish_complete_env_subcommand)"

if set -q is_gnu
    complete -c env -n __fish_env_not_yet_vars -s i -l ignore-environment -d "Start with an empty environment"
    complete -c env -n __fish_env_not_yet_vars -s u -l unset -d "Unset environment variable" -x -a "(set --names -x)"
    complete -c env -n __fish_env_not_yet_vars -l help -d "Display help and exit"
    complete -c env -n __fish_env_not_yet_vars -l version -d "Display version and exit"
else
    complete -c env -n __fish_env_not_yet_vars -s 0 -d "End output lines with NUL"
    complete -c env -n __fish_env_not_yet_vars -s i -d "Start with empty environment"
    complete -c env -n __fish_env_not_yet_vars -s P -d "Provide an alternate PATH"
    complete -c env -n __fish_env_not_yet_vars -s S -d "Split argument into args on ' '"
    complete -c env -n __fish_env_not_yet_vars -s u -d "Unset environment variable" -x -a "(set --names -x)"
    complete -c env -n __fish_env_not_yet_vars -s v -d "Verbose output on processing"
end
