# Completion for dive: https://github.com/wagoodman/dive

# Options
complete -c dive -l ci -d "Skip the interactive TUI and validate against CI rules"
complete -c dive -l ci-config -F -r -d "If CI=true in the environment, use the given yaml to drive validation rules"
complete -c dive -l config -F -r -d "Config file"
complete -c dive -s h -l help -d "Help for dive"
complete -c dive -l highestUserWastedPercent -r -n "__fish_seen_argument -l ci" -d "Highest allowable percentage of bytes wasted"
complete -c dive -l highestWastedBytes -r -n "__fish_seen_argument -l ci" -d "Highest allowable bytes wasted"
complete -c dive -s i -l ignore-errors -d "Ignore image parsing errors and run the analysis anyway"
complete -c dive -s j -l json -r -d "Skip the interactive TUI and write the layer analysis statistics to a given file"
complete -c dive -l lowestEfficiency -r -d "Lowest allowable image efficiency"
complete -c dive -l source -a "docker podman docker-archive" -d "The container engine to fetch the image from"
complete -c dive -s v -l version -d "Display version number"

# Subcommands
complete -c dive -a "build help version"
complete -c dive -n __fish_use_subcommand -xa build -d "Build and analyze a Docker image from a Dockerfile"
complete -c dive -n __fish_use_subcommand -xa help -d "Help about any command"
complete -c dive -n __fish_use_subcommand -xa version -d "Print the version number and exit"
complete -c dive -n "__fish_seen_subcommand_from help" -a "build help version"

# Arguments
complete -c dive -xa "(docker images --format '{{.Repository}}:{{.Tag}}' | command grep -v '<none>')"
