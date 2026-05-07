# Completion for dive: https://github.com/wagoodman/dive

# Builtin options and subcommands
dive completion fish | source

# Arguments
complete -c dive -xa "(docker images --format '{{.Repository}}:{{.Tag}}' | command grep -v '<none>')"
