function __fish_usbip_no_subcommand -d 'Test if usbip has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i attach detach list bind unbind port
            return 1
        end
    end
    return 0
end

function __fish_usbip_remote -d 'List remote usbip host'
    set -l remote (ip r | awk '/via/ {print $3"\t"$5}')
    printf '%s\n' $remote
end

function __fish_usbip_busid -d 'List usbip busid'
    set -l remote (commandline -xpc | string match -r '(?<=-r)(emote)?=?\s*(\S+)' | string trim)
    set -l busids (usbip list -r $remote 2> /dev/null | string match -r '\d+-\d+')
    printf '%s\n' $busids
end

function __fish_usbip_busid_attached -d 'List usbip busid attached'
    set -l usbids (usbip port 2> /dev/null | string match -r '(?<=Port\s)(\d+)')
    printf '%s\n' $usbids
end

complete -f -c usbip
complete -f -c usbip -n __fish_usbip_no_subcommand -a attach -d 'Attach a USB device to the host'
complete -f -c usbip -n __fish_usbip_no_subcommand -a detach -d 'Detach a USB device from the host'
complete -f -c usbip -n __fish_usbip_no_subcommand -a list -d 'List USB devices on the host'
complete -f -c usbip -n __fish_usbip_no_subcommand -a bind -d 'Bind a USB device to a driver'
complete -f -c usbip -n __fish_usbip_no_subcommand -a unbind -d 'Unbind a USB device from a driver'
complete -f -c usbip -n __fish_usbip_no_subcommand -a port -d 'List USB ports on the host'

# attach
complete -f -c usbip -n '__fish_seen_subcommand_from attach' -s r -l remote -d 'The machine with exported USB devices' -xa "(__fish_usbip_remote)"
complete -f -c usbip -n '__fish_seen_subcommand_from attach' -s b -l busid -d 'Busid of the device on <host>' -xa "(__fish_usbip_busid)"
complete -f -c usbip -n '__fish_seen_subcommand_from attach' -s d -l device -d 'Id of the virtual UDB on <host>'

# detach
complete -f -c usbip -n '__fish_seen_subcommand_from detach' -s p -l port -d 'Vhci_hcd port the device is on' -xa "(__fish_usbip_busid_attached)"

# list
complete -f -c usbip -n '__fish_seen_subcommand_from list' -s p -l parsable -d 'Parsable list format'
complete -f -c usbip -n '__fish_seen_subcommand_from list' -s r -l remote -d 'List the exportable USB devices on <host>' -xa "(__fish_usbip_remote)"
complete -f -c usbip -n '__fish_seen_subcommand_from list' -s l -l local -d 'List the local USB devices'
complete -f -c usbip -n '__fish_seen_subcommand_from list' -s d -l device -d 'List the local USB gadgets bound to usbip-vudc'

# Bind
complete -f -c usbip -n '__fish_seen_subcommand_from bind' -s b -l busid -d 'Bind usbip-host.ko to device on <busid>'

# unbind
complete -f -c usbip -n '__fish_seen_subcommand_from unbind' -s b -l busid -d 'Unbind usbip-host.ko to device on <busid>'
