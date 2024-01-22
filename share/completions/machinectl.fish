complete -f -c machinectl

set -l commands list status show start login enable disable poweroff reboot \
    terminate kill bind copy-{to,from} list-images image-status show-image clone rename read-only remove set-limit pull-{tar,raw,dkr} \
    {import,export}-{raw,tar} {list,cancel}-transfers shell

function __fish_systemd_has_machine_image
    set -l images (__fish_systemd_machine_images)
    for i in $images ".host"
        # Include ".host" here because it _is_ a valid machine
        if contains -- $i (commandline -xpc)
            echo $i
            return 0
        end
    end
    return 1
end

function __fish_systemd_has_machine
    set -l cmd
    if not count $argv >/dev/null
        set cmd (commandline -xpc)
    else
        set cmd $argv
    end
    set -l machines (__fish_systemd_machines)
    for m in $machines
        if contains -- $m $cmd
            echo $m
            return 0
        end
    end
    return 1
end

# General options
complete -x -c machinectl -s H -l host -d 'Execute the operation on a remote host' -a "(__fish_print_hostnames)"
complete -x -c machinectl -s M -l machine -d 'Execute operation on a VM or container' -a "(__fish_systemd_machines)"
complete -f -c machinectl -s h -l help -d 'Print a short help and exit'
complete -f -c machinectl -l version -d 'Print a short version and exit'
complete -f -c machinectl -l no-pager -d 'Do not pipe output into a pager'
complete -f -c machinectl -l no-legend -d 'Do not print header and footer'
complete -f -c machinectl -l no-ask-password -d "Don't ask for password for privileged operations" # might only be valid for certain commands, manpage is vague

# Machine commands
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a list -d "List running machines"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a list-images -d "List startable machine images"
complete -f -c machinectl -n "__fish_seen_subcommand_from list-images" -s a -l all -d "Also show machines starting with a '.'"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a status -d "Show information about machine"
complete -f -c machinectl -n "__fish_seen_subcommand_from status" -s l -l full -d "Do not ellipsize process tree entries"
complete -x -c machinectl -n "__fish_seen_subcommand_from status" -s n -l lines -d "How many journal lines to show"
complete -x -c machinectl -n "__fish_seen_subcommand_from status" -s o -l output -d "Formatting of journal output" -a \
    'short short-iso short-precise short-monotonic verbose export json json-pretty json-sse cat'

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a show -d "Show properties of machines"
complete -x -c machinectl -n "__fish_seen_subcommand_from show" -s p -l property -d "Limit to certain properties"
complete -f -c machinectl -n "__fish_seen_subcommand_from show" -s a -l all -d "Show all properties, even if not set"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a start -d "Start machine"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a login -d "Login to a machine"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a enable -d "Enable machine to start at boot"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a disable -d "Disable machine from starting at boot"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a poweroff -d "Poweroff machine"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a reboot -d "Reboot machine"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a terminate -d "Terminate machine (without shutting down)"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a kill -d "Send signal to process in a machine"
complete -x -c machinectl -n "__fish_seen_subcommand_from kill" -l kill-who -d "Choose who to send the signal to" -a 'leader all'
__fish_make_completion_signals
complete -x -c machinectl -n "__fish_seen_subcommand_from kill" -s s -l signal -d "Signal to send" -a "$__kill_signals"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a bind -d "Bind-mount a directory to a machine"
complete -f -c machinectl -n "__fish_seen_subcommand_from bind" -l mkdir -d "Create destination directory"
complete -f -c machinectl -n "__fish_seen_subcommand_from bind" -l read-only -d "Apply read-only mount"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a copy-to -d "Copy file or directory to a machine"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a copy-from -d "Copy file or directory from a machine"

complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a shell -d "Open a shell on a machine"

# Image commands
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a list-images -d "Show a list of locally installed machines"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a image-status -d "Show information about machine images (human-readable)"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a show-image -d "Show properties of machine images (machine-readable)"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a clone -d "Clone a machine image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a rename -d "Rename a machine image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a read-only -d "Mark or unmark machine image as read-only"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a remove -d "Remove machine images"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a set-limit -d "Set size limit for machine image"

# Image transfer commands
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a pull-tar -d "Download a .tar container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a pull-raw -d "Download a .raw container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a pull-dkr -d "Download a .dkr container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a import-tar -d "Import a .tar container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a import-raw -d "Import a .raw container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a export-tar -d "Export a .tar container image"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a export-raw -d "Export a .raw container image"
complete -f -c machinectl -n "__fish_seen_subcommand_from export-tar export-raw" -l format -d "Specify compression format" -a 'uncompressed xz gzip bzip2'
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a list-transfers -d "Show running downloads, imports and exports"
complete -f -c machinectl -n "not __fish_seen_subcommand_from $commands" -a cancel-transfers -d "Abort running downloads, imports or exports"
complete -x -c machinectl -n "__fish_seen_subcommand_from pull-{tar,raw}" -l verify -a 'no checksum signature' -d "Verify image with specified method"
complete -x -c machinectl -n "__fish_seen_subcommand_from pull-dkr" -l verify -a no -d "Verify image (not available for dkr)"
complete -x -c machinectl -n "__fish_seen_subcommand_from pull-dkr" -l dkr-index-url -d "Specify index server"
complete -f -c machinectl -n "__fish_seen_subcommand_from pull-{tar,raw,dkr}" -l force -d "Overwrite existing machine image"

# These take a list of _running_ machines
complete -f -c machinectl -n "__fish_seen_subcommand_from status show poweroff reboot terminate kill" -a "(__fish_systemd_machines)"

# These take a list of _installed_ machines, i.e. "machine images"
# enable seems to do something with a running machine that's not listed as an image, it seems to be a bug
# disable is kinda hard, as it's essentially a wrapper around `systemctl disable systemd-nspawn@XXX`, but completing machine images is the least wrong for now
complete -f -c machinectl -n "__fish_seen_subcommand_from start clone rename image-status show-image remove enable disable" -a "(__fish_systemd_machine_images)"

# These take one machine and then something else
# bind copy-to copy-from take paths so we're done (almost, copy-from takes a path _on the machine_),
# while login takes exactly one machine and nothing else, which fish currently doesn't do
complete -f -c machinectl -n "__fish_seen_subcommand_from login bind copy-to copy-from; and not __fish_systemd_has_machine" -a "(__fish_systemd_machines)"

complete -f -c machinectl -n "__fish_seen_subcommand_from login bind copy-to copy-from shell; and not __fish_systemd_has_machine" -a "(__fish_systemd_machines)"
# This is imperfect as we print the _local_ users
complete -f -c machinectl -n "__fish_seen_subcommand_from shell; and not __fish_systemd_has_machine (commandline -xpc | cut -d"@" -f2-)" -a "(__fish_print_users)@(__fish_systemd_machines)"

complete -f -c machinectl -n "__fish_seen_subcommand_from read-only; and not __fish_systemd_has_machine_image" -a "(__fish_systemd_machine_images)"
complete -f -c machinectl -n "__fish_seen_subcommand_from read-only; and __fish_systemd_has_machine_image" -a "yes no"
