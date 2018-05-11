function __fish_print_addresses --description "Print a list of known network addresses"
    if command -sq ip
        command ip --oneline address | string replace -r '(\S+\s+){3}(.+)/.*' '$2'
    else if command -sq ifconfig
        # This is for OSX/BSD
        # There's also linux ifconfig but that has at least two different output formats
        # is basically dead, and ip is installed on everything now
        ifconfig | awk '/^\tinet/ { print $2 } '
    end
end

