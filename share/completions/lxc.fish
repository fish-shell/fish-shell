function __fish_lxc_no_subcommand -d 'Test if lxc has yet to be given the command'
    for i in (commandline --tokenize --cut-at-cursor --current-process)
        if contains -- $i config copy delete exec file help image launch list move network profile publish remote restore restart snapshot start stop
            return 1
        end
    end
    return 0
end

function __fish_lxc_list_containers
    lxc list -c n | string match -r '\| [a-zA-Z_-]+' | string replace -r '\| ' ''
end

complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments config --description 'Manage configuration.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments copy --description 'Copy containers within or in between lxd instances.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments delete --description 'Delete containers or snapshots.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments exec --description 'Execute the specified command in a container.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments file --description 'Manage files on a container.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments finger --description 'Check if the LXD instance is up.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments help --description 'Print help.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments image --description 'Manipulate container images.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments info --description 'List information on LXD servers and containers.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments init --description 'Initialize a container from a particular image.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments launch --description 'Launch a container from a particular image.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments list --description 'Lists the available resources.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments manpage --description 'Prints all the subcommands help.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments monitor --description 'Monitor activity on the LXD server.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments move --description 'Move containers within or in between lxd instances.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments network --description 'Manage networks.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments pause --description 'Changes state of one or more containers to pause.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments profile --description 'Manage configuration profiles.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments publish --description 'Publish containers as images.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments remote --description 'Manage remote LXD servers.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments restart --description 'Changes state of one or more containers to restart.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments restore --description 'Set the current state of a container back to a snapshot.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments snapshot --description 'Create a read-only snapshot of a container.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments start --description 'Changes state of one or more containers to start.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments stop --description 'Changes state of one or more containers to stop.'
complete --condition '__fish_lxc_no_subcommand' --command lxc --no-files --arguments version --description 'Prints the version number of this client tool.'

# config
complete --condition '__fish_seen_subcommand_from config' --command lxc --no-files --arguments "device get set unset show edit trust"

# exec
complete --condition '__fish_seen_subcommand_from exec' --command lxc --no-files --arguments "(__fish_lxc_list_containers)"

# start
complete --condition '__fish_seen_subcommand_from start' --command lxc --no-files --arguments "(__fish_lxc_list_containers)"

# stop
complete --condition '__fish_seen_subcommand_from stop' --command lxc --no-files --arguments "(__fish_lxc_list_containers)"
