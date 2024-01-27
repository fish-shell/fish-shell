# iw(8) completion for fish

# The completions for iw are based off those for ip, which has a similar options structure.

set -l iw_commands dev phy reg event features commands list
# wdev not supported since it is barely documented

function __fish_iw_device
    __fish_print_interfaces | while read -l i
        printf '%s\t%s\n' $i Device
    end
end

function __fish_iw_phy
    set -l phys /sys/class/ieee80211/*
    string match --regex '[^/]*$' $phys
end

function __fish_iw_ssid
    printf '%s\n' (iw dev $argv[1] scan dump | string match --regex --entire SSID | string split ' ')[2]
end

function __fish_complete_iw
    set -l cmd (commandline -xpc)

    if string match --quiet -- '-*' $cmd[2]
        set -e cmd[2] # Allow other completions to complete as normal
    end
    if not set -q cmd[2]
        return # Uses completions from $iw_commands
    else if not set -q cmd[3]
        switch $cmd[2]
            case dev
                __fish_iw_device
            case phy
                __fish_iw_phy
        end
        return
    end
    switch $cmd[2]
        case dev
            switch "$cmd[4]"
                case ap
                    if not set -q cmd[5]
                        printf '%s\t%s\n' stop "Stop AP functionality" \
                            start "Start AP functionality"
                    end
                case auth connect
                    if not set -q cmd[5]
                        __fish_iw_ssid $cmd[3]
                    end
                case disconnect
                case cqm
                    if not set -q cmd[5]
                        echo rssi
                    else if not set -q cmd[6]
                        echo threshold
                        echo off
                    end
                case ftm
                    if not set -q cmd[5]
                        printf '%s\t%s\n' start_responder "Start an FTM responder" \
                            get_stats "Get FTM responder statistics"
                    end
                case ibss
                    if not set -q cmd[5]
                        printf '%s\t%s\n' join "Join an IBSS cell or create one" \
                            leave "Leave the current IBSS cell"
                    end
                case switch
                    if not set -q cmd[5]
                        echo channel
                        echo freq
                    end
                case info del
                case interface
                    if not set -q cmd[5]
                        echo add
                    end
                case link
                case measurement
                    if not set -q cmd[5]
                        echo ftm_request
                    else if not set -q cmd[6]
                        __fish_complete_path
                    end
                    # options after config file?
                case mesh
                    if not set -q cmd[5]
                        printf '%s\t%s\n' leave "Leave a mesh" \
                            join "Join a mesh"
                    end
                case mgmt
                    if not set -q cmd[5]
                        echo dump
                    else if not set -q cmd[6]
                        echo frame
                    end
                case mpath
                    if not set -q cmd[5]
                        printf '%s\t%s\n' dump "List known mesh paths" \
                            set "Set an existing mesh path's next hop" \
                            new "Create a new mesh math manually" \
                            del "Remove the mesh path to a node" \
                            get "Get information on a mesh path to a node" \
                            probe "Inject ethernet frame to a peer"
                    end
                case mpp
                    if not set -q cmd[5]
                        printf '%s\t%s\n' dump "List known mesh proxy paths" \
                            get "Get information on mesh proxy path to a node"
                    end
                case ocb
                    if not set -q cmd[5]
                        printf '%s\t%s\n' leave "Leave the OCB mode network" \
                            join "Join the OCB mode network"
                    end
                case cac
                    if not set -q cmd[5]
                        echo channel
                        echo freq
                        echo trigger
                    end
                case roc
                    if not set -q cmd[5]
                        echo start
                    end
                case scan
                    if not set -q cmd[5]
                        printf '%s\t%s\n' sched_stop "Stop an ongoing scheduled scan" \
                            sched_start "Start a scheduled scan" \
                            abort "Abort ongoing scan" \
                            trigger "Trigger a scan" \
                            dump "Dmp the current scan results"
                    end
                    # -u option?
                case set
                    if not set -q cmd[5]
                        printf '%s\t%s\n' bitrates "Rate masks" \
                            mcast_rate "Multicast bitrate" \
                            peer "WDS peer" \
                            noack_map "Noack map for TIDs" \
                            4addr "4addr (WDS) mode" \
                            type "Interface type/mode" \
                            meshid "" \
                            monitor "Monitor flags" \
                            mesh_param "Mesh parameter" \
                            txpower "Transmit power" \
                            channel "" \
                            freq "" \
                            power_save "Power save state"
                    else
                        switch "$cmd[5]"
                            case type
                                if not set -q cmd[6]
                                    printf '%s\n' managed ibss monitor mesh wds
                                end
                            case channel
                                if not set -q cmd[6]
                                    # cmd[6] is just the simple channel number
                                else if not set -q cmd[7]
                                    printf '%s\n' NOHT HT20 HT40+ HT40- 5MHz 10MHz 80MHz 160MHz
                                end
                        end
                    end
                case get
                    if not set -q cmd[5]
                        printf '%s\t%s\n' mesh_param "Mesh parameter" \
                            power_save "Power save state"
                    end
                case station
                    if not set -q cmd[5]
                        printf '%s\t%s\n' dump "List all stations known" \
                            set "Set station options" \
                            del "Remove a station entry" \
                            get "Get information for a station"
                    end
                case survey
                    if not set -q cmd[5]
                        echo dump
                    end
                case vendor
                    if not set -q cmd[5]
                        echo recvbin
                        echo recv
                        echo send
                    end
                case '*'
                    printf '%s\t%s\n' ap "Start/stop AP functionality" \
                        auth "Authenticate with a network" \
                        connect "Join a network" \
                        disconnect "Disconnect from the current network" \
                        cqm "Set connection quality monitor RSSI threshold" \
                        ftm "Control the FTM responder" \
                        ibss "Join or leave a IBSS cell" \
                        switch "Switch the operating channel by sending a CSA" \
                        info "Show information for this interface" \
                        del "Remove this virtual interface" \
                        interface "Control the interface" \
                        link "Print information about the current link" \
                        measurement "Perform measurements" \
                        mesh "Join or leave a mesh" \
                        mpp "Get information on mesh proxy paths" \
                        mgmt "Print mgmt frames" \
                        mpath "Interact with mesh paths" \
                        ocb "Join or leave an OCB mode network" \
                        offchannel "Leave operating channel" \
                        cac "Start or trigger a channel availability chec" \
                        roc "" \
                        scan "Scan and probe for SSIDs" \
                        set "Set parameters" \
                        get "Get parameters" \
                        station "Set station parameters" \
                        survey "List all gathered channel survey data" \
                        vendor ""
            end
        case phy
            switch "$cmd[4]"
                case coalesce
                    if not set -q cmd[5]
                        printf '%s\t%s\n' show "Show coalesce status" \
                            disable "Disable coalesce" \
                            enable "Enable coalesce"
                    else if test "$cmd[5]" = enable && not set -q cmd[6]
                        __fish_complete_path # Enable takes a config file
                    end
                case hwsim
                    if not set -q cmd[5]
                        printf '%s\n' wakequeues stopqueues setps getps
                    end
                case info
                case interface
                    if not set -q cmd[5]
                        echo add
                    end
                case channels
                case reg
                    if not set -q cmd[5]
                        printf '%s\t%s\n' get "Print the current regulatory domain information" \
                            set "Notify kernel about current regulatory domain"
                    end
                case set
                    if not set -q cmd[5]
                        printf '%s\t%s\n' txq "TXQ parameters" \
                            antenna "Bitmap of allowed antennas" \
                            txpower "Transmit power" \
                            distance "Link distance coverage class" \
                            netns "Network namespace" \
                            retry "Retry limit" \
                            rts "Rts threshold" \
                            frag "Fragmentation threshold" \
                            channel "" \
                            name "Wireless device name"
                    end
                case get
                    if not set -q cmd[5]
                        printf '%s\t%s\n' txq "TXQ parameters"
                    end
                case get
                    if not set -q cmd[5]
                        printf '%s\t%s\n' show "Show WoWLAN status" \
                            disable "Disable WoWLAN" \
                            enable "Enable WoWLAN"
                    end
                case '*'
                    printf '%s\t%s\n' coalesce "Interface with coalesce" \
                        hwsim "" \
                        info "Show wireless device capabilities" \
                        interface "Add a new virtual interface" \
                        channels "Show available channels" \
                        reg "Manage regulatory domains" \
                        set "Set parameters" \
                        get "Get parameters"
            end
    end
end

complete -f -c iw
complete -f -c iw -a '(__fish_complete_iw)'
complete -f -c iw -n "not __fish_seen_subcommand_from $iw_commands" -a "$iw_commands"
# Yes, iw only takes options before "objects"
complete -f -c iw -l debug -d "Enable netlink message debugging" -n "not __fish_seen_subcommand_from $iw_commands"
complete -f -c iw -l version -d "Print the version" -n "not __fish_seen_subcommand_from $iw_commands"

complete -f -c iw -n "__fish_seen_subcommand_from event" -s t -d 'Print timestamp'
complete -f -c iw -n "__fish_seen_subcommand_from event" -s r -d 'Print relative timestamp'
complete -f -c iw -n "__fish_seen_subcommand_from event" -s f -d 'Print full frame for auth/assoc'

complete -f -c iw -n "__fish_seen_subcommand_from reg" -a reload -d 'Reload the kernel\'s regulatory database'
complete -f -c iw -n "__fish_seen_subcommand_from reg" -a get -d 'Print the kernel\'s current regulatory domain information'
complete -f -c iw -n "__fish_seen_subcommand_from reg" -a set -d 'Notify the kernel about the current regulatory domain'
