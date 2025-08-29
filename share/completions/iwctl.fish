# Execute an `iwctl ... list` command and parse output
function __iwctl_list
    set -l python (__fish_anypython); and command -q busctl; or return
    $python -c "
import subprocess, json, sys, getopt
SERVICE = 'net.connman.iwd'
DEVICE = f'{SERVICE}.Device'
def call(*a): return json.loads(subprocess.run(['busctl','call','--json=short',f'{SERVICE}',*a],stdout=subprocess.PIPE).stdout)['data'][0]
objects=call('/','org.freedesktop.DBus.ObjectManager','GetManagedObjects')
dev = lambda x: x[DEVICE]['Name']['data']

def names(i): [print(v[i]['Name']['data']) for o,v in objects.items() if i in v]
def devices(i): [print(dev(v)) for o,v in objects.items() if i in v and DEVICE in v]
def networks(name: str):
    station = next(obj for obj,i in objects.items() if f'{SERVICE}.Station' in i and dev(i)==name)
    for obj, signal in call(station, 'net.connman.iwd.Station', 'GetOrderedNetworks'):
        network = objects[obj][f'{SERVICE}.Network']
        print(network['Name']['data'], network['Type']['data'], signal, sep='\t')

opts, args = getopt.getopt(sys.argv[1:], 'dn')
for opt, _ in opts:
    if opt == '-d': devices(f'{SERVICE}.{args[0]}'); break
    if opt == '-n': networks(args[0]); break
else: names(f'{SERVICE}.{args[0]}') " $argv 2>/dev/null
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
    for network in (__iwctl_list -n $argv[3])
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
complete -c iwctl -n '__iwctl_match_subcoms ad-hoc' -a "(__iwctl_list -d AdHoc)"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a start -d "Start or join an Ad-Hoc network"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a start_open -d "Start of join an open Ad-Hoc network"
complete -c iwctl -n '__iwctl_match_subcoms "ad-hoc *"' -n 'not __iwctl_match_subcoms ad-hoc list' -a stop -d "Leave an Ad-Hoc network"

# adapter
complete -c iwctl -n '__iwctl_match_subcoms adapter' -a list -d "List adapters"
complete -c iwctl -n '__iwctl_match_subcoms adapter' -a "(__iwctl_list Adapter)"
complete -c iwctl -n '__iwctl_match_subcoms "adapter *"' -n 'not __iwctl_match_subcoms adapter list' -a show -d "Show adapter info"
complete -c iwctl -n '__iwctl_match_subcoms "adapter *"' -n 'not __iwctl_match_subcoms adapter list' -a set-property -d "Set property"
# TODO implement completions for `properties`, i.e. all rows with `*` in first column

# ap
complete -c iwctl -n '__iwctl_match_subcoms ap' -a list -d "List devices in AP mode"
complete -c iwctl -n '__iwctl_match_subcoms ap' -a "(__iwctl_list -d AccessPoint)"
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
complete -c iwctl -n '__iwctl_match_subcoms device' -a "(__iwctl_list Device)"
complete -c iwctl -n '__iwctl_match_subcoms "device *"' -n 'not __iwctl_match_subcoms device list' -a show -d "Show device info"
complete -c iwctl -n '__iwctl_match_subcoms "device *"' -n 'not __iwctl_match_subcoms device list' -a set-property -d "Set property"
# TODO implement completions for `properties`, i.e. all rows with `*` in first column

# dpp
complete -c iwctl -n '__iwctl_match_subcoms dpp' -a list -d "List DPP-capable devices"
complete -c iwctl -n '__iwctl_match_subcoms dpp' -a "(__iwctl_list -d DeviceProvisioning)"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a start-enrollee -d "Starts a DPP Enrollee"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a start-configurator -d "Starts a DPP Configurator"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a stop -d "Aborts a DPP operations"
complete -c iwctl -n '__iwctl_match_subcoms "dpp *"' -n 'not __iwctl_match_subcoms dpp list' -a show -d "Show the DPP state"

# known-networks
# TODO Does not support SSIDs ending/starting on whitespace. Not sure how to fix.
complete -c iwctl -n '__iwctl_match_subcoms known-networks' -a list -d "List known networks"
complete -c iwctl -n '__iwctl_match_subcoms known-networks' -a "(__iwctl_list KnownNetwork)"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a forget -d "Forget a known network"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a show -d "Show known network"
complete -c iwctl -n '__iwctl_match_subcoms "known-networks *"' -n 'not __iwctl_match_subcoms known-networks list' -a set-property -d "Set property"

# station
complete -c iwctl -n '__iwctl_match_subcoms station' -a list -d "List devices in Station mode"
complete -c iwctl -n '__iwctl_match_subcoms station' -a "(__iwctl_list -d Station)"
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
complete -c iwctl -n '__iwctl_match_subcoms wsc' -a "(__iwctl_list -d SimpleConfiguration)"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a push-button -d "PushButton Mode"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a start-user-pin -d "PIN mode"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a start-pin -d "PIN mode with generated PIN"
complete -c iwctl -n '__iwctl_match_subcoms "wsc *"' -n 'not __iwctl_match_subcoms wsc list' -a cancel -d "Aborts WSC operations"
