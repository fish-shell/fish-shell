# Completions for vagrant, a vm/container management thing.
# Docs are at https://www.vagrantup.com/docs/cli/.
#
# Apparently options can only come after commands, with the exception of "-v" and "-h", which are effectively commands.
set -l commands box cloud connect destroy \
    docker-{exec,logs,run} \
    global-status halt help init list-commands login \
    package plugin provision push rdp reload resume \
    rsync rsync-auto share snapshot ssh ssh-config \
    status suspend up version \
    port powershell winrm{,-config}

set -l box_commands add help list outdated prune remove repackage update
set -l cloud_commands auth box search provider publish version
set -l plugin_commands install license list uninstall update
set -l snapshot_commands delete list pop push restore save

function __fish_print_vagrant_state
    # Find a .vagrant file/directory above $PWD
    set -l root
    set -l dir (pwd -P)
    while test $dir != /
        if test -d $dir.vagrant -o -f $dir.vagrant
            echo $dir.vagrant
            return 0
        end
        # Go up one directory
        set dir (string replace -r '[^/]*/?$' '' $dir)
    end
    return 1
end

function __fish_vagrant_machines
    if set -l state (__fish_print_vagrant_state)
        test -d "$state"; or return
        set -l machines $state/machines/*
        string replace -- $state/machines/ '' $machines
    end
end

function __fish_vagrant_boxes
    set -l vhome $VAGRANT_HOME/boxes
    set -q vhome[1]; or set vhome ~/.vagrant.d/boxes
    set -l boxes $vhome/*
    string replace -- $vhome/ '' $boxes | string replace -- -VAGRANTSLASH- /
end

function __fish_vagrant_need_command -V commands
    argparse -s h/help v/version -- (commandline -xpc)[2..-1] 2>/dev/null
    or return
    if set -q _flag_help[1]; or set -q _flag_help[1]
        return 1
    end
    if set -q argv[1]
        echo $argv
        return 1
    end
    return 0
end

function __fish_vagrant_using_command
    set -l cmd (__fish_vagrant_need_command)
    contains -- $cmd[1] $argv
end

function __fish_vagrant_box_need_command
    set -l cmd (__fish_vagrant_need_command)
    test "$cmd[1]" = box 2>/dev/null
    or return 1
    set -e cmd[1]

    # Not all of these are valid for all subcommands, but that's not important here.
    # Yes, none of these have a short version.
    set -l opts b-box-version= c-cacert= C-capath= 1-cert= 2-clean f-force i-insecure p-provider=
    set -a opts 3-checksum= 4-checksum-type= n-name=
    set -a opts g-global d-dry-run a-all B-box
    argparse -s $opts -- $cmd 2>/dev/null
    or return 1
    if set -q argv[1]
        echo $argv
        return 1
    end
    return 0
end

function __fish_vagrant_cloud_need_command
    set -l cmd (__fish_vagrant_need_command)
    test "$cmd[1]" = cloud 2>/dev/null
    or return 1
    set -e cmd[1]

    set -l opts c-check l-logout t-token d-description= s-short-description= p-private b-box-version=
    set -a opts f-force r-release u-url v-version-description= P-page= S-short= o-order= L-limit= 1-sort-by=
    argparse -s $opts -- $cmd 2>/dev/null
    or return 1
    if set -q argv[1]
        echo $argv
        return 1
    end
    return 0
end

complete -c vagrant -n __fish_vagrant_need_command -fa box -d 'Manage boxes: installation, removal, etc.'
complete -c vagrant -n "__fish_vagrant_using_command box; and __fish_vagrant_box_need_command" -fa "$box_commands"
complete -c vagrant -n __fish_vagrant_need_command -fa cloud -d 'Manage Vagrant Cloud'
complete -c vagrant -n "__fish_vagrant_using_command cloud; and __fish_vagrant_cloud_need_command" -fa "$cloud_commands"
complete -c vagrant -n __fish_vagrant_need_command -fa connect
complete -c vagrant -n __fish_vagrant_need_command -fa destroy -d 'Stop and delete all traces of the vagrant machine'
complete -c vagrant -n __fish_vagrant_need_command -fa docker-exec
complete -c vagrant -n __fish_vagrant_need_command -fa docker-logs
complete -c vagrant -n __fish_vagrant_need_command -fa docker-run
complete -c vagrant -n __fish_vagrant_need_command -fa global-status -d 'Show status Vagrant environments for this user'
complete -c vagrant -n __fish_vagrant_need_command -fa halt -d 'Stop a machine'
complete -c vagrant -n __fish_vagrant_need_command -fa help -d 'Show help for a subcommand'
complete -c vagrant -n __fish_vagrant_need_command -fa init -d 'Initialize a new Vagrant env by creating a Vagrantfile'
complete -c vagrant -n "__fish_vagrant_using_command init" -fa '(__fish_vagrant_boxes)'
complete -c vagrant -n __fish_vagrant_need_command -fa list-commands
complete -c vagrant -n __fish_vagrant_need_command -fa login
complete -c vagrant -n __fish_vagrant_need_command -fa package -d 'Package a running vagrant environment into a box'
complete -c vagrant -n __fish_vagrant_need_command -fa plugin -d 'Manages plugins: install, uninstall, update, etc.'
complete -c vagrant -n __fish_vagrant_need_command -fa provision
complete -c vagrant -n __fish_vagrant_need_command -fa push
complete -c vagrant -n __fish_vagrant_need_command -fa rdp
complete -c vagrant -n __fish_vagrant_need_command -fa reload
complete -c vagrant -n __fish_vagrant_need_command -fa resume
complete -c vagrant -n __fish_vagrant_need_command -fa rsync
complete -c vagrant -n __fish_vagrant_need_command -fa rsync-auto
complete -c vagrant -n __fish_vagrant_need_command -fa share
complete -c vagrant -n __fish_vagrant_need_command -fa snapshot
complete -c vagrant -n __fish_vagrant_need_command -fa ssh -d 'Connect to a machine via SSH'
complete -c vagrant -n __fish_vagrant_need_command -fa ssh-config -d 'Print ssh config to connect to machine'
complete -c vagrant -n __fish_vagrant_need_command -fa status -d 'Print status of a machine'
complete -c vagrant -n __fish_vagrant_need_command -fa suspend -d 'Suspend a machine'
complete -c vagrant -n __fish_vagrant_need_command -fa up -d 'Start and provision the environment'
complete -c vagrant -n "__fish_vagrant_using_command up" -fa '(__fish_vagrant_machines)'
complete -c vagrant -n __fish_vagrant_need_command -fa version -d 'Print current and latest version'
