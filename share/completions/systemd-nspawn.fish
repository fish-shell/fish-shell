complete -c systemd-nspawn -s D -l directory -d 'Directory to use as file system root for the container' -r
complete -c systemd-nspawn -l template -d 'Directory or btrfs subvolume to use as template' -r
complete -c systemd-nspawn -s x -l ephemeral -d 'Run container with a temporary btrfs snapshot (Only on btrfs)'
complete -c systemd-nspawn -s i -l image -d 'Disk image to mount the root directory for the container from' -r
complete -c systemd-nspawn -s b -l boot -d 'Invoke init in the container'
complete -c systemd-nspawn -s u -l user -d 'Change to user in the container' -x
complete -c systemd-nspawn -s M -l machine -d 'Sets the machine name for this container' -x
complete -c systemd-nspawn -l uuid -d 'Set the specified UUID for the container.'
complete -c systemd-nspawn -l slice -d 'Make the container part of the specified slice'
complete -c systemd-nspawn -l property -d 'Set a unit property on the scope unit'
complete -c systemd-nspawn -l private-users -d 'Enables user namespacing'
complete -c systemd-nspawn -l private-network -d 'Disconnect networking of the container from the host'
complete -c systemd-nspawn -l network-interface -d 'Assign the specified network interface to the container' -x
complete -c systemd-nspawn -l network-macvlan -d 'Create a "macvlan" interface of the specified Ethernet interface' -x
complete -c systemd-nspawn -l network-ipvlan -d 'Create an "ipvlan" interface of the specified Ethernet interface' -x
complete -c systemd-nspawn -s n -l network-veth -d 'Create a virtual Ethernet link ("veth") between host and container' -x
complete -c systemd-nspawn -l network-bridge -d 'Adds the host side of the Ethernet link created to the specified bridge' -x
complete -c systemd-nspawn -s p -l port -d 'Map an ip port from the host to the container' -x
complete -c systemd-nspawn -s Z -l selinux-context -d 'Sets the SELinux security context'
complete -c systemd-nspawn -s L -l selinux-apifs-context -d 'Sets the SELinux security context for files in the API filesystems'
complete -c systemd-nspawn -l capability -d 'Grant additional capabilities to the container'
complete -c systemd-nspawn -l drop-capability -d 'Drop capabilities from the container'
complete -c systemd-nspawn -l kill-signal -d "Signal to send to the container's PID1 when nspawn receives SIGTERM"
complete -c systemd-nspawn -l link-journal -d 'Set container journal visibility' -a 'no host try-host guest try-guest auto' -r -A
complete -c systemd-nspawn -s j -d 'Equivalent to --link-journal=try-guest'
complete -c systemd-nspawn -l read-only -d 'Mount the root file system read-only for the container'
complete -c systemd-nspawn -l bind -l bind-ro -d 'Bind mount a file or directory from the host in the container' -r
complete -c systemd-nspawn -l tmpfs -d 'Mount a tmpfs file system into the container' -r
complete -c systemd-nspawn -l overlay -l overlay-ro -d 'Mount directories as overlayfs in the container' -r
complete -c systemd-nspawn -l setenv -d 'Pass environment variables to init in the container'
complete -c systemd-nspawn -l share-system -d 'Share the system with the host [See Man Page]'
complete -c systemd-nspawn -l register -d 'Register container with systemd-machined' -a "yes no" -r
complete -c systemd-nspawn -l keep-unit -d 'Only register the unit with systemd-machined'
complete -c systemd-nspawn -l personality -d 'The architecture reported by uname in the container'
complete -c systemd-nspawn -s q -l quiet -d 'Turns off any status output by the tool itself'
complete -c systemd-nspawn -l volatile -d 'Boots the container in volatile mode'
complete -c systemd-nspawn -s h -l help -d 'Print a short help text and exit'
complete -c systemd-nspawn -l version -d 'Print a short version string and exit'
# Not quite correct, but otherwise we'd need to get the machine root, and we can't always know that
complete -c systemd-nspawn -n "__fish_not_contain_opt -s b boot" -a "(__fish_complete_subcommand)" -f

