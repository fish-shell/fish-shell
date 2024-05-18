# Execute an `iwctl ... list` command and parse output
function __iwctl_filter -w iwctl
    # set results "iwctl $cmd list | tail -n +5"
    # if test -n "$empty"
    #     set -a results "| string match --invert '*$empty*'"
    # end
    # eval "$results" | awk '{print $2}'
    # awk does not work on multiline entries, therefor we use string match,
    # which has the added benefit of filtering out the `No devices in ...` lines

    argparse -i all-columns -- $argv

    # remove color escape sequences
    set -l results (iwctl $argv | string replace -ra '\e\[[\d;]+m' '')
    # calculate column widths
    set -l headers $results[3]
    # We exploit the fact that all column labels will have >2 space to the left, and inside column labels there is always only one space.
    set -l leading_ws (string match -r "^ *" -- $headers | string length)
    set -l column_widths (string match -a -r '(?<=  )\S.*?(?:  (?=\S)|$)' -- $headers | string length)

    if set -ql _flag_all_columns
        for line in (string match "  *" -- $results[5..] | string sub -s (math $leading_ws + 1))
            for column_width in $column_widths
                printf %s\t (string sub -l $column_width -- $line | string trim -r)
                set line (string sub -s (math $column_width + 1) -- $line)
            end
            printf "\n"
        end
    else if set -q column_widths[1]
        # only take lines starting with `  `, i.e., no `No devices ...`
        # then take the first column as substring
        string match "  *" $results[5..] | string sub -s (math $leading_ws + 1) -l $column_widths[1] | string trim -r
    end
end

function __iwctl_match_subcoms
    set -l match (string split --no-empty " " -- $argv)

    set argv (commandline -pxc)
    # iwctl allows to specify arguments for username, password, passphrase and dont-ask regardless of any following commands
    argparse -i 'u/username=' 'p/password=' 'P/passphrase=' v/dont-ask -- $argv
    set argv $argv[2..]

    if test (count $argv) != (count $match)
        return 1
    end

    while set -q argv[1]
        string match -q -- $match[1] $argv[1]
        or return 1
        set -e match[1] argv[1]
    end
end

function __iwctl_connect
    set argv (commandline -pxc)
    # remove all options
    argparse -i 'u/username=' 'p/password=' 'P/passphrase=' v/dont-ask -- $argv
    # station name should now be the third argument (`iwctl station <wlan>`)
    for network in (__iwctl_filter station $argv[3] get-networks rssi-dbms --all-columns)
        set network (string split \t -- $network)
        set -l strength "$network[3]"
        # This follows iwctls display of * to ****
        # https://git.kernel.org/pub/scm/network/wireless/iwd.git/tree/client/station.c?id=4a0a97379008489daa108c9bc0a4204c1ae9c6a8#n380
        if test $strength -ge -6000
            set strength 4
        else if test $strength -ge -6700
            set strength 3
        else if test $strength -ge -7500
            set strength 2
        else
            set strength 1
        end

        printf "%s\t[%s] - %s\n" "$network[1]" (string repeat -n $strength '*' | string pad -rw 4 -c -) "$network[2]"
    end
end

# The `empty` messages in case we want to go back to using those
# set ad_hoc '(__iwctl_filter ad-hoc "No devices in Ad-Hoc mode available.")'
# set adpater '(__iwctl_filter adapter)'
# set ap '(__iwctl_filter ap "No devices in access point mode available.")'
# set device '(__iwctl_filter device)'
# set dpp '(__iwctl_filter dpp "No DPP-capable devices available")'
# set known_networks '(__iwctl_filter known-networks)'
# set station '(__iwctl_filter station "No devices in Station mode available.")'
# set wsc '(__iwctl_filter wsc "No WSC-capable devices available")'

complete -f iwctl

# Options
complete -c iwctl -s h -l help
complete -c iwctl -s p -l password -rf
complete -c iwctl -s u -l username -rf
complete -c iwctl -s P -l passphrase -rf
complete -c iwctl -s v -l dont-ask -d "Don't ask for missing credentials"

# Subcommand
complete -c iwctl -n __iwctl_match_subcoms \
    -a "ad-hoc adapter ap debug device dpp exit help known-networks quit station version wsc"

# ad-hoc
complete -c iwctl -n '__iwctl_match_subcoms ad-hoc' -a list -d "List devices in Ad-Hoc mode"
complete -c iwctl -n '__iwctl_match_subcoms ad-hoc' -a "(__iwctl_filter ad-hoc list)"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a start -d "Start or join an Ad-Hoc network"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a start_open -d "Start of join an open Ad-Hoc network"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a stop -d "Leave an Ad-Hoc network"

# adapter
complete -c iwctl -n '__iwctl_match_subcoms adapter' -a list -d "List adapters"
complete -c iwctl -n '__iwctl_match_subcoms adapter' -a "(__iwctl_filter adapter list)"
complete -c iwctl -n '__iwctl_match_subcoms "adapter *"' -n 'not __iwctl_match_subcoms adapter list' -a show -d "Show adapter info"
complete -c iwctl -n '__iwctl_match_subcoms "adapter *"' -n 'not __iwctl_match_subcoms adapter list' -a set-property -d "Set property"
# TODO implement completions for `properties`, i.e. all rows with `*` in first column

# ap
complete -c iwctl -n '__iwctl_match_subcoms ap' -a list -d "List devices in AP mode"
complete -c iwctl -n '__iwctl_match_subcoms ap' -a "(__iwctl_filter ap list)"
complete -c iwctl -n '__iwctl_match_subcoms "ap *"' -n 'not __iwctl_match_subcoms ap list' -a start -d "Start an access point"
complete -c iwctl -n '__iwctl_match_subcoms "ap *"' -n 'not __iwctl_match_subcoms ap list' -a start-profile -d "Start an access point based on a disk profile"
complete -c iwctl -n '__iwctl_match_subcoms "ap *"' -n 'not __iwctl_match_subcoms ap list' -a stop -d "Stop a started access point"
complete -c iwctl -n '__iwctl_match_subcoms "ap *"' -n 'not __iwctl_match_subcoms ap list' -a show -d "Show AP info"
complete -c iwctl -n '__iwctl_match_subcoms "ap *"' -n 'not __iwctl_match_subcoms ap list' -a get-networks -d "Get network list after scanning"

# debug
complete -c iwctl -n '__iwctl_match_subcoms "debug *"' -a connect -d "Connect to a specific BSS"
complete -c iwctl -n '__iwctl_match_subcoms "debug *"' -a roam -d "Roam to a BSS"
complete -c iwctl -n '__iwctl_match_subcoms "debug *"' -a get-networks -d "Get networks"
complete -c iwctl -n '__iwctl_match_subcoms "debug *"' -a autoconnect -d "Set autoconnect property"
complete -c iwctl -n '__iwctl_match_subcoms "debug * autoconnect"' -a "on off" -d "Set autoconnect property"

# device
complete -c iwctl -n '__iwctl_match_subcoms device' -a list -d "List devices"
complete -c iwctl -n '__iwctl_match_subcoms device' -a "(__iwctl_filter device list)"
complete -c iwctl -n '__iwctl_match_subcoms "device *"' -n 'not __iwctl_match_subcoms device list' -a show -d "Show device info"
complete -c iwctl -n '__iwctl_match_subcoms "device *"' -n 'not __iwctl_match_subcoms device list' -a set-property -d "Set property"
# TODO implement completions for `properties`, i.e. all rows with `*` in first column

# dpp
complete -c iwctl -n '__iwctl_match_subcoms dpp' -a list -d "List DPP-capable devices"
complete -c iwctl -n '__iwctl_match_subcoms dpp' -a "(__iwctl_filter dpp list)"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a start-enrollee -d "Starts a DPP Enrollee"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a start-configurator -d "Starts a DPP Configurator"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a stop -d "Aborts a DPP operations"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a show -d "Show the DPP state"

# known-networks
# TODO Does not support SSIDs ending/starting on whitespace. Not sure how to fix.
complete -c iwctl -n '__iwctl_match_subcoms known-networks' -a list -d "List known networks"
complete -c iwctl -n '__iwctl_match_subcoms known-networks' -a "(__iwctl_filter known-networks list)"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a forget -d "Forget a known network"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a show -d "Show nown network"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a set-property -d "Set property"

# station
complete -c iwctl -n '__iwctl_match_subcoms station' -a list -d "List devices in Station mode"
complete -c iwctl -n '__iwctl_match_subcoms station' -a "(__iwctl_filter station list)"
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a connect -d "Connect to network"
complete -c iwctl -n '__iwctl_match_subcoms "station * connect"' -a "(__iwctl_connect)" -d "Connect to network" --keep-order
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a connect-hidden -d "Connect to hidden network"
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a disconnect -d Disconnect
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a get-networks -d "Get networks"
complete -c iwctl -n '__iwctl_match_subcoms "station * get-networks"' -a "rssi-dbms rssi-bars"
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a get-hidden-access-points -d "Get hidden APs"
complete -c iwctl -n '__iwctl_match_subcoms "station * get-hidden-access-points"' -a rssi-dbms
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a scan -d "Scan for networks"
complete -c iwctl -n '__iwctl_match_subcoms "station *"' -n 'not __iwctl_match_subcoms station list' -a show -d "Show station info"

# wsc
complete -c iwctl -n '__iwctl_match_subcoms wsc' -a list -d "List WSC-capable devices"
complete -c iwctl -n '__iwctl_match_subcoms wsc' -a "(__iwctl_filter wsc list)"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a push-button -d "PushButton Mode"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a start-user-pin -d "PIN mode"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a start-pin -d "PIN mode with generated PIN"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a cancel -d "Aborts WSC operations"
