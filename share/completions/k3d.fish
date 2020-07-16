# Disable file completion
complete -c k3d -f

# Only suggest the following commands if none have been used yet
complete -rc k3d -n __fish_use_subcommand -a create -d "Create a resource [cluster, node]"
complete -rc k3d -n __fish_use_subcommand -a delete -d "Delete a resource [cluster, node]"
complete -rc k3d -n __fish_use_subcommand -a get -d "Get a resource [cluster, node, kubeconfig]"
complete -rc k3d -n __fish_use_subcommand -a help -d "Help menu"
complete -rc k3d -n __fish_use_subcommand -a load -d "Load an in image into a cluster"
complete -rc k3d -n __fish_use_subcommand -a start -d "Start a resource [cluster, node]"
complete -rc k3d -n __fish_use_subcommand -a stop -d "Stop a resource [cluster, node]"
complete -rc k3d -n __fish_use_subcommand -a version -d "Print k3d version"
complete -rc k3d -n __fish_use_subcommand -a --help -d "Show usage help"
