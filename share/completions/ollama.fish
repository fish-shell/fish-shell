function __fish_ollama_list
    ollama list 2>/dev/null | tail -n +2 | string replace --regex "\s.*" ""
end

complete -c ollama -a serve -d "Start ollama"
complete -c ollama -a create -d "Create a model from a Modelfile"
complete -c ollama -a show -d "Show information for a model"
complete -c ollama -a run -d "Run a model"
complete -c ollama -a pull -d "Pull a model from a registry"
complete -c ollama -a push -d "Push a model to a registry"
complete -c ollama -a list -d "List models"
complete -c ollama -a cp -d "Copy a model"
complete -c ollama -a rm -d "Remove a model"
complete -c ollama -a help -d "Help about any command"
complete -c ollama -s h -l help -d "help for ollama"
complete -c ollama -s v -l version -d "Show version information"
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from show'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from run'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from cp'
complete -c ollama -f -a "(__fish_ollama_list)" --condition '__fish_seen_subcommand_from rm'
