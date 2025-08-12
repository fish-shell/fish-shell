set -l commands assemble create rm enter list ls stop upgrade ephemeral generate-entry version help

complete -c distrobox -f

function __fish_distrobox_list_containers
    distrobox ls | tr -d ' ' | cut -d "|" -f2 | tail -n +2
end

complete -c distrobox \
    -s V -l version -d "Show version"
complete -c distrobox -n "__fish_seen_subcommand_from $commands" \
    -s v -l verbose -d "Show more verbosity"
complete -c distrobox -n "__fish_seen_subcommand_from $commands" \
    -s h -l help -d "Show help message"
complete -c distrobox -n "__fish_seen_subcommand_from enter ephemeral list ls create rm generate-entry upgrade && not __fish_seen_subcommand_from assemble" \
    -s r -l root -d "Launch podman/docker/lilipod with root privileges"

complete -c distrobox -n "not __fish_seen_subcommand_from $commands" \
    -a "$commands"

complete -c distrobox -n "__fish_seen_subcommand_from assemble" \
    -a "create rm"
complete -c distrobox -n "__fish_seen_subcommand_from assemble && __fish_seen_subcommand_from create rm" \
    -l file -F -d "Path or URL to the distrobox manifest/ini file"
complete -c distrobox -n "__fish_seen_subcommand_from assemble && __fish_seen_subcommand_from create rm" \
    -s n -l name -d "Run against a single entry in the manifest/ini file"
complete -c distrobox -n "__fish_seen_subcommand_from assemble && __fish_seen_subcommand_from create rm" \
    -s r -l replace -d "Replace already existing distroboxes with matching names"
complete -c distrobox -n "__fish_seen_subcommand_from assemble && __fish_seen_subcommand_from create rm" \
    -s d -l dry-run -d "Only print the container manager command generated"

complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s i -l image -ra "(distrobox create -C)"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s n -l name -d "Image to use for the container"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l hostname -d "Hostname for the distrobox"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s p -l pull -d "Pull the image even if it exists locally (implies --yes)"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral stop && not __fish_seen_subcommand_from assemble" \
    -s Y -l yes -d "non-interactive, pull images without asking"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s c -l clone -ra "(__fish_distrobox_list_containers)" -d "Name of the distrobox container to use as base for a new container"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s H -l home -d "Select a custom HOME directory for the container"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l volume -d "Additional volumes to add to the container"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s a -l additional-flags -d "Additional flags to pass to the container manager command"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s ap -l additional-packages -d "Additional packages to install during initial container setup"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l init-hooks -d "Additional commands to execute at the end of container initialization"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l pre-init-hooks -d "Additional commands to execute at the start of container initialization"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s I -l init -d "Use init system (like systemd) inside the container."
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l nvidia -d "Try to integrate host's nVidia drivers in the guest"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l platform -d "Specify which platform to use, eg: linux/arm64"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-devsys -d "Do not share host devices and sysfs dirs from host"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-groups -d "Do not forward user's additional groups into the container"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-ipc -d "Do not share ipc namespace with host"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-netns -d "Do not share the net namespace with host"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-process -d "Do not share process namespace with host"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l unshare-all -d "Activate all the unshare flags"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -s C -l compatibility -d "Show list of compatible images"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l no-entry -d "Do not generate a container entry in the application list"
complete -c distrobox -n "__fish_seen_subcommand_from create ephemeral && not __fish_seen_subcommand_from assemble" \
    -l absolutely-disable-root-password-i-am-really-positively-sure -d "When setting up a rootful distrobox, this will skip user password setup, leaving it blank"

complete -c distrobox -n "__fish_seen_subcommand_from enter rm stop upgrade" \
    -ra "(__fish_distrobox_list_containers)"
complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -s n -l name -ra "(__fish_distrobox_list_containers)" -d "Name for the distrobox"
complete -c distrobox -n "__fish_seen_subcommand_from enter ephemeral" \
    -s "e -" -d "End arguments, execute the rest as command to execute at login"
complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -l clean-path -d "Reset PATH inside container to FHS standard"
complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -s T -l no-tty -d "Do not instantiate a tty"
complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -s nw -l no-workdir -d "Always start the container from container's home directory"
complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -s a -l additional-flags -d "Additional flags to pass to the container manager command"

complete -c distrobox -n "__fish_seen_subcommand_from enter" \
    -s d -l dry-run -d "Only print the container manager command generated"

complete -c distrobox -n "__fish_seen_subcommand_from list ls" \
    -l no-color -d "Disable color formatting"

complete -c distrobox -n "__fish_seen_subcommand_from rm stop upgrade generate-entry" \
    -s a -l all -d "Delete all distroboxes"
complete -c distrobox -n "__fish_seen_subcommand_from rm" \
    -s f -l force -d "Force deletion"
complete -c distrobox -n "__fish_seen_subcommand_from rm" \
    -l rm-home -d "Remove the mounted home if it differs from the host user's one"

complete -c distrobox -n "__fish_seen_subcommand_from upgrade" \
    -l running -d "Perform only for running distroboxes"

complete -c distrobox -n "__fish_seen_subcommand_from generate-entry" \
    -s d -l delete -d "Delete the entry"
complete -c distrobox -n "__fish_seen_subcommand_from generate-entry" \
    -s i -l icon -F -r -d "Specify a custom icon"
