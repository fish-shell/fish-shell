function __fish_k3d_no_subcommand --description 'Test if k3d has yet to be given subcommands'
    for i in (commandline -xpc)
        if contains -- $i cluster image kubeconfig node version
            return 1
        end
    end
    return 0
end

function __fish_k3d_print_clusters --description 'Print a list of k3d clusters'
    k3d cluster list --no-headers | awk '{print $1}'
end

function __fish_k3d_print_nodes --description 'Print a list of k3d nodes'
    k3d node list --no-headers | awk '{print $1}'
end

complete -f -c k3d -n __fish_k3d_no_subcommand -s h -l help -d "More information about a command"
complete -f -c k3d -n __fish_k3d_no_subcommand -l version -d "Show k3d and default k3s version"
complete -f -c k3d -n __fish_k3d_no_subcommand -a cluster -d "Manage cluster(s)"
complete -f -c k3d -n __fish_k3d_no_subcommand -a version -d "Show k3d and default k3s version"
complete -f -c k3d -n __fish_k3d_no_subcommand -a image -d "Handle container images"
complete -f -c k3d -n __fish_k3d_no_subcommand -a kubeconfig -d "Manage kubeconfig(s)"
complete -f -c k3d -n __fish_k3d_no_subcommand -a node -d "Manage node(s)"

complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -a create -d "Create a new cluster"
complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -a list -d "List cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -a delete -d "Delete cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -a start -d "Start existing k3d cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -a stop -d "Stop existing k3d cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from cluster; and __fish_seen_subcommand_from delete start stop' -a '(__fish_k3d_print_clusters)' -d Cluster
complete -f -c k3d -n '__fish_seen_subcommand_from cluster' -s h -l help -d "Print usage"

complete -f -c k3d -n '__fish_seen_subcommand_from image' -a import -d "Import image(s) from docker into k3d cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from image' -a help -d "Print usage"
complete -f -c k3d -n '__fish_seen_subcommand_from image' -s h -l help -d "Print usage"

complete -f -c k3d -n '__fish_seen_subcommand_from kubeconfig' -a get -d "Get kubeconfig from cluster(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from kubeconfig' -a merge -d "Merge/write kubeconfig(s) from cluster(s) into existing kubeconfig/file"
complete -f -c k3d -n '__fish_seen_subcommand_from kubeconfig get merge' -a '(__fish_k3d_print_clusters)' -d Cluster
complete -f -c k3d -n '__fish_seen_subcommand_from kubeconfig get merge' -s a -l all -d "Get kubeconfig from all existing clusters"
complete -f -c k3d -n '__fish_seen_subcommand_from kubeconfig' -s h -l help -d "Print usage"

complete -f -c k3d -n '__fish_seen_subcommand_from node' -a create -d "Create a new k3s node in docker"
complete -f -c k3d -n '__fish_seen_subcommand_from node' -a delete -d "Delete node(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from node' -a list -d "List node(s)"
complete -f -c k3d -n '__fish_seen_subcommand_from node' -a start -d "Start an existing k3d node"
complete -f -c k3d -n '__fish_seen_subcommand_from node' -a stop -d "Stop an existing k3d node"
complete -f -c k3d -n '__fish_seen_subcommand_from node; and __fish_seen_subcommand_from delete start stop' -a '(__fish_k3d_print_nodes)' -d Node
complete -f -c k3d -n '__fish_seen_subcommand_from node' -s h -l help -d "Print usage"
