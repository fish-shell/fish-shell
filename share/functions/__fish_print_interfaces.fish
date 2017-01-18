function __fish_print_interfaces --description "Print a list of known network interfaces"
    if test -d /sys/class/net
        for i in /sys/class/net/*
            basename $i
        end
    else # OSX/BSD
        command ifconfig -l | string split ' '
    end
end
