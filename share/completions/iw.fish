# iw(8) completion for fish

# The completions for iw are based off those for ip, which has a similar options structure.

set -l iw_commands dev phy reg event features commands

function __fish_iw_device
    __fish_print_interfaces | while read i
        printf '%s\t%s\n' $i "Device"
    end
end

function __fish_complete_iw
    set -l cmd (commandline -opc)
    if not set -q cmd[2]
        return # Use completions from $iw_commands
    else if not set -q cmd[3]
        switch $cmd[2]
            case dev
                __fish_iw_device
            case phy
                /usr/bin/env ls /sys/class/ieee80211
        end
    else
        switch $cmd[2]
            case dev
                switch "$cmd[4]"
                    case ap
                        if not set -q cmd[5]
                            printf '%s\t%s\n' stop "Stop AP functionality" \
                            start "Start AP functionality"
                        end
                    case auth
                        # -w option?
                    case connect disconnect
                    case cqm
                        if not set -q cmd[5]
                            echo rssi
                        else if not set -q cmd[6]
                            echo treshold
                            echo off
                        end
                    case ftm
                        if not set -q cmd[5]
                            printf '%s\t%s\n' start_responder "Start an FTM responder" \
                            get_status "Get FTM responder statistics"
                        end
                    case ibss
                        if not set -q cmd[5]
                            printf '%s\t%s\n' join "Join an IBSS cell or create one" \
                            leave "Leave the current IBSS cell"
                        end
                    case 'switch'
                        if not set -q cmd[5]
                            echo channel
                            echo freq
                        end
                    case info del
                    case interface
                        if not set -q cmd[5]
                            echo add
                        end
                        # more options?
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
                        # What's this?
                    case scan
                        # -u option?
                        # and sched_start and sched_stop and abort and trigger and dump  commands
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
                        'switch' "Switch the operating channel by sending a CSA" \
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
                            disable "Enable coalesce"
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
                        interface "Add a new virutal interface" \
                        channels "Show available channels" \
                        reg "Manage regulatory domains" \
                        set "Set parameters" \
                        get "Get parameters" \
                end
            end
        end
    end
end

complete -f -c iw
complete -f -c iw -a '(__fish_complete_iw)'
complete -f -c iw -n "not __fish_seen_subcommand_from $iw_commands" -a "$iw_commands"
# # Yes, iw only takes options before "objects"
complete -f -c iw -l debug -d "Enable netlink message debugging" -n "not __fish_seen_subcommand_from $iw_commands"
complete -f -c iw -l version -d "Print the version" -n "not __fish_seen_subcommand_from $iw_commands"

complete -f -c iw -n "__fish_seen_subcommand_from event" -s t -d 'Print timestamp'
complete -f -c iw -n "__fish_seen_subcommand_from event" -s r -d 'Print relative timestamp'
complete -f -c iw -n "__fish_seen_subcommand_from event" -s f -d 'Print full frame for auth/assoc'
