function __fish_print_addresses --description "List own network addresses with interface as description"
    # if --all is given, also print 0.0.0.0 and ::
    if contains -- --all $argv
        echo -e "0.0.0.0\tall"
        echo -e "::\tall"
    end
    if command -sq ip
        command ip --oneline address | string replace -r '^\d+:\s+(\S+)\s+inet6?\s+([^\s/]+).*' '$2\t$1'
    else if command -sq ifconfig
        # This is for OSX/BSD
        # There's also linux ifconfig but that has at least two different output formats
        # is basically dead, and ip is installed on everything now
        ifconfig | awk '/^\tinet/ { print $2 } '
    end
end
