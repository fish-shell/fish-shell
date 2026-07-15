function __fish_scutil_flag
    set -l toks (commandline -xpc)
    set -e toks[1]
    for t in $toks
        if string match -qr '^-' -- $t
            echo $t
            return 0
        end
    end
    return 1
end

function __fish_scutil_no_flag
    not __fish_scutil_flag >/dev/null
end

function __fish_scutil_flag_is
    set -l f (__fish_scutil_flag)
    string length -q -- $f
    and test "$f" = "$argv[1]"
end

function __fish_scutil_nc_no_verb
    __fish_scutil_flag_is --nc; or return 1
    set -l toks (commandline -xpc)
    for v in list status show statistics select start stop suspend resume ondemand trigger \
        enablevpn disablevpn help
        if contains -- $v $toks
            return 1
        end
    end
    return 0
end

function __fish_scutil_nc_verb_is
    __fish_scutil_flag_is --nc; or return 1
    contains -- $argv[1] (commandline -xpc)
end

# Output line format: ...  "Service Name"  [VPN:type]
function __fish_scutil_nc_services
    scutil --nc list |
        string replace -rf '.*"([^"]+)"\s*(\[.*\])?\s*$' '$1'
end

complete -c scutil -n __fish_scutil_no_flag -l prefs -d 'Open SCPreferences interactive CLI (optional preference-file)'
complete -c scutil -n __fish_scutil_no_flag -s r -d 'Check network reachability of a host, IP address, or address pair'
complete -c scutil -n __fish_scutil_no_flag -s w -d 'Wait for a dynamic store key to exist (optional -t timeout)'
complete -c scutil -n __fish_scutil_no_flag -l get -d 'Report the value of a system preference'
complete -c scutil -n __fish_scutil_no_flag -l set -d 'Update a system preference (requires root)'
complete -c scutil -n __fish_scutil_no_flag -l dns -d 'Report the current DNS configuration'
complete -c scutil -n __fish_scutil_no_flag -l proxy -d 'Report the current proxy configuration'
complete -c scutil -n __fish_scutil_no_flag -l nc -d 'Monitor and interact with VPN connections'
complete -c scutil -n __fish_scutil_no_flag -l renew -d 'Trigger re-evaluation of network configuration on an interface'

complete -c scutil -n '__fish_scutil_flag_is -r' -s W -d 'Watch: report reachability status as network configuration changes'

complete -c scutil -n '__fish_scutil_flag_is -w' -s t -d 'Timeout in seconds (0 = wait indefinitely; default 15)'

complete -c scutil -f -n '__fish_scutil_flag_is --get' -a ComputerName -d 'User-friendly system name'
complete -c scutil -f -n '__fish_scutil_flag_is --get' -a LocalHostName -d 'Local (Bonjour) host name'
complete -c scutil -f -n '__fish_scutil_flag_is --get' -a HostName -d 'Name associated with hostname(1) / gethostname(3)'
complete -c scutil -f -n '__fish_scutil_flag_is --set' -a ComputerName -d 'User-friendly system name'
complete -c scutil -f -n '__fish_scutil_flag_is --set' -a LocalHostName -d 'Local (Bonjour) host name'
complete -c scutil -f -n '__fish_scutil_flag_is --set' -a HostName -d 'Name associated with hostname(1) / gethostname(3)'

complete -c scutil -f -n __fish_scutil_nc_no_verb -a list -d 'List available VPN services in the current set'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a status -d 'Show connection status and extended info for a service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a show -d 'Display configuration for a service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a statistics -d 'Show byte/packet/error statistics for a service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a select -d 'Make a service active in the current set'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a start -d 'Start a service (optional --user/--password/--secret)'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a stop -d 'Stop a service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a suspend -d 'Suspend a service (PPP, Modem on Hold)'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a resume -d 'Resume a suspended service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a ondemand -d 'Display VPN on-demand information'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a trigger -d 'Trigger VPN on-demand for a hostname (optional port/background)'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a enablevpn -d 'Enable a VPN application type or service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a disablevpn -d 'Disable a VPN application type or service'
complete -c scutil -f -n __fish_scutil_nc_no_verb -a help -d 'Show available --nc commands'

for v in status show statistics select start stop suspend resume enablevpn disablevpn
    complete -c scutil -x -n "__fish_scutil_nc_verb_is $v" -a '(__fish_scutil_nc_services)'
end

complete -c scutil -n '__fish_scutil_nc_verb_is start' -l user -d 'User name for VPN authentication'
complete -c scutil -n '__fish_scutil_nc_verb_is start' -l password -d 'Password for VPN authentication'
complete -c scutil -n '__fish_scutil_nc_verb_is start' -l secret -d 'Shared secret for VPN authentication'

complete -c scutil -n '__fish_scutil_nc_verb_is ondemand' -s W -d 'Watch: report on-demand changes as they occur'
