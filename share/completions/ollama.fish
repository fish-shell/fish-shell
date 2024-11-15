function __fish_ollama_list
    ollama list 2>/dev/null | tail -n +2 | string replace --regex "\s.*" ""
end


complete -f -c ollama
complete -c ollama -n __fish_use_subcommand -a serve -d "Start ollama"
complete -c ollama -n __fish_use_subcommand -a create -d "Create a model from a Modelfile"
complete -c ollama -n __fish_use_subcommand -a show -d "Show information for a model"
complete -c ollama -n __fish_use_subcommand -a run -d "Run a model"
complete -c ollama -n __fish_use_subcommand -a pull -d "Pull a model from a registry"
complete -c ollama -n __fish_use_subcommand -a push -d "Push a model to a registry"
complete -c ollama -n __fish_use_subcommand -a list -d "List models"
complete -c ollama -n __fish_use_subcommand -a cp -d "Copy a model"
complete -c ollama -n __fish_use_subcommand -a rm -d "Remove a model"
complete -c ollama -n __fish_use_subcommand -a help -d "Help about any command"
complete -c ollama -s h -l help -d "help for ollama"
complete -c ollama -s v -l version -d "Show version information"
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from show'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from run'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from cp'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from rm'
