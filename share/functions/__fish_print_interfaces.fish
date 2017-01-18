function __fish_print_interfaces --description "Print a list of known network interfaces"
    if test -d /sys/class/net
        string replace /sys/class/net/ '' /sys/class/net/*
    else # OSX/BSD
        command ifconfig -l | string split ' '
    end
end
