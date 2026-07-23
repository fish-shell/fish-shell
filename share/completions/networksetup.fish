# networksetup - configure macOS network settings (man 8 networksetup)
#
# networksetup uses single-dash long verbs (e.g. `networksetup -getinfo Wi-Fi`).
# The first dashed token is the verb; most verbs take a <networkservice>,
# <hardwareport|device>, <location>, <bond>, or PPPoE <service> as their first
# operand, which we enumerate live.

# ── command-line introspection ───────────────────────────────────────
# Print the verb (first dashed token after the command), without its dash.
function __fish_networksetup_verb
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        if string match -q -- '-*' $t
            string trim -l -c - -- $t
            return 0
        end
    end
    return 1
end

# True when no verb has been chosen yet (so verbs should be offered).
function __fish_networksetup_no_verb
    not __fish_networksetup_verb >/dev/null 2>&1
end

# ── live enumerators ─────────────────────────────────────────────────
function __fish_networksetup_services
    networksetup -listallnetworkservices 2>/dev/null | tail -n +2 \
        | string replace -r '^\*\s*' ''
end

function __fish_networksetup_hardwareports
    networksetup -listallhardwareports 2>/dev/null \
        | string replace -rf '^Hardware Port: ' ''
end

function __fish_networksetup_devices
    networksetup -listallhardwareports 2>/dev/null \
        | string replace -rf '^Device: ' ''
end

function __fish_networksetup_locations
    networksetup -listlocations 2>/dev/null
end

function __fish_networksetup_bonds
    networksetup -listBonds 2>/dev/null
end

function __fish_networksetup_pppoe
    networksetup -listpppoeservices 2>/dev/null
end

# ── verbs ────────────────────────────────────────────────────────────
complete -c networksetup -n __fish_networksetup_no_verb -o addDeviceToBond -d 'Add the specified device to the specified bond.'
complete -c networksetup -n __fish_networksetup_no_verb -o addpreferredwirelessnetworkatindex -d 'Add wireless network to a device preferred list at an index'
complete -c networksetup -n __fish_networksetup_no_verb -o connectpppoeservice -d 'Connect the PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o create6to4service -d 'Create a 6 to 4 service with name <newnetworkservicename>.'
complete -c networksetup -n __fish_networksetup_no_verb -o createBond -d 'Create a new bond and add the specified devices'
complete -c networksetup -n __fish_networksetup_no_verb -o createlocation -d 'Create a new network location with the specified name.'
complete -c networksetup -n __fish_networksetup_no_verb -o createnetworkservice -d 'Create a service on a hardware port'
complete -c networksetup -n __fish_networksetup_no_verb -o createpppoeservice -d 'Create a PPPoE service on the specified device'
complete -c networksetup -n __fish_networksetup_no_verb -o createVLAN -d 'Create a VLAN over a device'
complete -c networksetup -n __fish_networksetup_no_verb -o deleteBond -d 'Delete the bond with the specified device-name.'
complete -c networksetup -n __fish_networksetup_no_verb -o deletelocation -d 'Delete the location.'
complete -c networksetup -n __fish_networksetup_no_verb -o deletepppoeservice -d 'Delete the PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o deleteVLAN -d 'Delete the VLAN over the parent device'
complete -c networksetup -n __fish_networksetup_no_verb -o detectnewhardware -d 'Detect new network hardware and create a default service'
complete -c networksetup -n __fish_networksetup_no_verb -o disconnectpppoeservice -d 'Disconnect the PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o duplicatenetworkservice -d 'Duplicate a network service under a new name'
complete -c networksetup -n __fish_networksetup_no_verb -o getadditionalroutes -d 'Get additional IPv4 routes associated with a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getairportnetwork -d 'Display current Wi-Fi Network for a device'
complete -c networksetup -n __fish_networksetup_no_verb -o getairportpower -d 'Display whether Wi-Fi power is on or off for a device'
complete -c networksetup -n __fish_networksetup_no_verb -o getautoproxyurl -d 'Display proxy auto-config (url, enabled) info for a service'
complete -c networksetup -n __fish_networksetup_no_verb -o getcomputername -d 'Display the computer name.'
complete -c networksetup -n __fish_networksetup_no_verb -o getcurrentlocation -d 'Display the name of the current location.'
complete -c networksetup -n __fish_networksetup_no_verb -o getdnsservers -d 'Display DNS info for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getinfo -d 'Display IP, subnet, router, and ethernet info for a service'
complete -c networksetup -n __fish_networksetup_no_verb -o getmacaddress -d 'Display ethernet or Wi-Fi address for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o getmedia -d 'Show current and active media on a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o getMTU -d 'Get the MTU value for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o getnetworkserviceenabled -d 'Display whether a service is on or off.'
complete -c networksetup -n __fish_networksetup_no_verb -o getproxyautodiscovery -d 'Display whether Proxy Auto Discover is on or off for a service'
complete -c networksetup -n __fish_networksetup_no_verb -o getproxybypassdomains -d 'Display Bypass Domain Names for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getsearchdomains -d 'Display Domain Name info for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getsecurewebproxy -d 'Display Secure Web proxy info for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getsocksfirewallproxy -d 'Display SOCKS Firewall proxy info for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getv6additionalroutes -d 'Get additional IPv6 routes associated with a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o getwebproxy -d 'Display Web proxy info for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o help -d 'Display these help listings.'
complete -c networksetup -n __fish_networksetup_no_verb -o isBondSupported -d 'Return whether the device can be added to a bond'
complete -c networksetup -n __fish_networksetup_no_verb -o listallhardwareports -d 'Display list of hardware ports with device name and address'
complete -c networksetup -n __fish_networksetup_no_verb -o listallnetworkservices -d 'Display list of network services'
complete -c networksetup -n __fish_networksetup_no_verb -o listBonds -d 'List all of the bonds.'
complete -c networksetup -n __fish_networksetup_no_verb -o listdevicesthatsupportVLAN -d 'List the devices that support VLANs.'
complete -c networksetup -n __fish_networksetup_no_verb -o listlocations -d 'List all of the locations.'
complete -c networksetup -n __fish_networksetup_no_verb -o listnetworkserviceorder -d 'Display services with port and device in connection order'
complete -c networksetup -n __fish_networksetup_no_verb -o listpppoeservices -d 'List all of the PPPoE services in the current set.'
complete -c networksetup -n __fish_networksetup_no_verb -o listpreferredwirelessnetworks -d 'List the preferred wireless networks for a device'
complete -c networksetup -n __fish_networksetup_no_verb -o listvalidmedia -d 'List valid media options for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o listvalidMTUrange -d 'List the valid MTU range for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o listVLANs -d 'List the VLANs that have been created.'
complete -c networksetup -n __fish_networksetup_no_verb -o ordernetworkservices -d 'Order the network services in the order specified'
complete -c networksetup -n __fish_networksetup_no_verb -o printcommands -d 'Displays a quick listing of commands.'
complete -c networksetup -n __fish_networksetup_no_verb -o removeallpreferredwirelessnetworks -d 'Remove all preferred wireless networks for a device'
complete -c networksetup -n __fish_networksetup_no_verb -o removeDeviceFromBond -d 'Remove the specified device from the specified bond'
complete -c networksetup -n __fish_networksetup_no_verb -o removenetworkservice -d 'Remove the named network service'
complete -c networksetup -n __fish_networksetup_no_verb -o removepreferredwirelessnetwork -d 'Remove a network from a device preferred list'
complete -c networksetup -n __fish_networksetup_no_verb -o renamenetworkservice -d 'Rename a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o set6to4automatic -d 'Set the service to get its 6 to 4 info automatically.'
complete -c networksetup -n __fish_networksetup_no_verb -o set6to4manual -d 'Set the service to get its 6 to 4 info manually.'
complete -c networksetup -n __fish_networksetup_no_verb -o setadditionalroutes -d 'Set additional IPv4 routes associated with a service'
complete -c networksetup -n __fish_networksetup_no_verb -o setairportnetwork -d 'Set Wi-Fi Network for a device'
complete -c networksetup -n __fish_networksetup_no_verb -o setairportpower -d 'Set Wi-Fi power for a device to on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setautoproxystate -d 'Set proxy auto-config to either on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setautoproxyurl -d 'Set proxy auto-config to a url for a service and enable it'
complete -c networksetup -n __fish_networksetup_no_verb -o setbootp -d 'Set the network service TCP/IP configuration to BOOTP.'
complete -c networksetup -n __fish_networksetup_no_verb -o setcomputername -d 'Set the computer name.'
complete -c networksetup -n __fish_networksetup_no_verb -o setdhcp -d 'Set the network service TCP/IP configuration to DHCP'
complete -c networksetup -n __fish_networksetup_no_verb -o setdnsservers -d 'Set the DNS servers for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setmanual -d 'Set a network service TCP/IP configuration to manual'
complete -c networksetup -n __fish_networksetup_no_verb -o setmanualwithdhcprouter -d 'Set a service to manual IP with a DHCP router'
complete -c networksetup -n __fish_networksetup_no_verb -o setmedia -d 'Set media for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o setMTU -d 'Set MTU for a hardware port or device'
complete -c networksetup -n __fish_networksetup_no_verb -o setMTUAndMediaAutomatically -d 'Set a hardware port or device back to automatic media/MTU'
complete -c networksetup -n __fish_networksetup_no_verb -o setnetworkserviceenabled -d 'Set a network service to on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setpppoeaccountname -d 'Set the account name for the specified PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o setpppoepassword -d 'Set the keychain password for the specified PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o setproxyautodiscovery -d 'Set Proxy Auto Discovery to either on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setproxybypassdomains -d 'Set the proxy bypass domains for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setsearchdomains -d 'Set the search domains for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setsecurewebproxy -d 'Set Secure Web proxy for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setsecurewebproxystate -d 'Set Secure Web proxy to either on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setsocksfirewallproxy -d 'Set SOCKS Firewall proxy for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setsocksfirewallproxystate -d 'Set SOCKS Firewall proxy to either on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o setv4off -d 'Turn IPv4 off on a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setv6additionalroutes -d 'Set additional IPv6 routes associated with a service'
complete -c networksetup -n __fish_networksetup_no_verb -o setv6automatic -d 'Set the service to get its IPv6 info automatically.'
complete -c networksetup -n __fish_networksetup_no_verb -o setv6LinkLocal -d 'Set the service to use its IPv6 only for link local.'
complete -c networksetup -n __fish_networksetup_no_verb -o setv6manual -d 'Set the service to get its IPv6 info manually.'
complete -c networksetup -n __fish_networksetup_no_verb -o setv6off -d 'Turn IPv6 off on a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setwebproxy -d 'Set Web proxy for a network service'
complete -c networksetup -n __fish_networksetup_no_verb -o setwebproxystate -d 'Set Web proxy to either on or off'
complete -c networksetup -n __fish_networksetup_no_verb -o showBondStatus -d 'Display the status of the specified bond.'
complete -c networksetup -n __fish_networksetup_no_verb -o showpppoestatus -d 'Display the status of the specified PPPoE service.'
complete -c networksetup -n __fish_networksetup_no_verb -o switchtolocation -d 'Make the specified location the current location.'
complete -c networksetup -n __fish_networksetup_no_verb -o version -d 'Display version of networksetup tool.'

# ── verb arguments (live enumeration) ────────────────────────────────
set -l svc_verbs getadditionalroutes getautoproxyurl getdnsservers getinfo \
    getnetworkserviceenabled getproxyautodiscovery getproxybypassdomains \
    getsearchdomains getsecurewebproxy getsocksfirewallproxy getv6additionalroutes \
    getwebproxy duplicatenetworkservice removenetworkservice renamenetworkservice \
    set6to4automatic set6to4manual setadditionalroutes setautoproxystate setautoproxyurl \
    setbootp setdhcp setdnsservers setmanual setmanualwithdhcprouter setnetworkserviceenabled \
    setproxyautodiscovery setproxybypassdomains setsearchdomains setsecurewebproxy \
    setsecurewebproxystate setsocksfirewallproxy setsocksfirewallproxystate setv4off \
    setv6additionalroutes setv6automatic setv6LinkLocal setv6manual setv6off setwebproxy \
    setwebproxystate ordernetworkservices
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $svc_verbs" \
    -a '(__fish_networksetup_services)'

set -l port_verbs getmacaddress getmedia getMTU listvalidmedia listvalidMTUrange \
    setmedia setMTU setMTUAndMediaAutomatically
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $port_verbs" \
    -a '(__fish_networksetup_hardwareports)'
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $port_verbs" \
    -a '(__fish_networksetup_devices)'

set -l dev_verbs getairportnetwork getairportpower setairportnetwork setairportpower \
    addpreferredwirelessnetworkatindex removeallpreferredwirelessnetworks \
    removepreferredwirelessnetwork listpreferredwirelessnetworks isBondSupported \
    addDeviceToBond removeDeviceFromBond createpppoeservice
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $dev_verbs" \
    -a '(__fish_networksetup_devices)'

set -l loc_verbs deletelocation switchtolocation
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $loc_verbs" \
    -a '(__fish_networksetup_locations)'

set -l bond_verbs deleteBond showBondStatus
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $bond_verbs" \
    -a '(__fish_networksetup_bonds)'

set -l pppoe_verbs connectpppoeservice disconnectpppoeservice deletepppoeservice \
    setpppoeaccountname setpppoepassword showpppoestatus
complete -c networksetup -x -n "contains -- (__fish_networksetup_verb) $pppoe_verbs" \
    -a '(__fish_networksetup_pppoe)'
