function __fish_lxc_no_subcommand -d 'Test if lxc has yet to be given the command'
    for i in (commandline --tokens-expanded --cut-at-cursor --current-process)
        if contains -- $i config console copy delete exec file help image info launch list move network pause profile publish remote rename restart restore shell snapshot start stop
            return 1
        end
    end
    return 0
end

function __fish_lxc_list_containers
    lxc list -c n | string match -r '\| [a-zA-Z0-9_-]+' | string replace -r '\| ' ''
end

complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments config -d 'Manage configuration.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments console -d 'Attach to instance console.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments copy -d 'Copy containers within or in between lxd instances.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments delete -d 'Delete containers or snapshots.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments exec -d 'Execute the specified command in a container.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments file -d 'Manage files on a container.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments finger -d 'Check if the LXD instance is up.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments help -d 'Print help.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments image -d 'Manipulate container images.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments info -d 'List information on LXD servers and containers.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments init -d 'Initialize a container from a particular image.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments launch -d 'Launch a container from a particular image.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments list -d 'Lists the available resources.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments manpage -d 'Prints all the subcommands help.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments monitor -d 'Monitor activity on the LXD server.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments move -d 'Move containers within or in between lxd instances.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments network -d 'Manage networks.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments pause -d 'Changes state of one or more containers to pause.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments profile -d 'Manage configuration profiles.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments publish -d 'Publish containers as images.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments remote -d 'Manage remote LXD servers.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments rename -d 'Rename instance or snapshot.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments restart -d 'Changes state of one or more containers to restart.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments restore -d 'Set the current state of a container back to a snapshot.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments shell -d 'Execute commands in instance.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments snapshot -d 'Create a read-only snapshot of a container.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments start -d 'Changes state of one or more containers to start.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments stop -d 'Changes state of one or more containers to stop.'
complete --condition __fish_lxc_no_subcommand --command lxc --no-files --arguments version -d 'Prints the version number of this client tool.'

# config
complete --condition '__fish_seen_subcommand_from config' --command lxc --no-files --arguments "device get set unset show edit trust"

set -l subcommands_taking_name console copy delete exec export info move pause publish rename restart restore shell snapshot start stop
complete --condition "__fish_seen_subcommand_from $subcommands_taking_name" --command lxc --no-files --arguments "(__fish_lxc_list_containers)"
