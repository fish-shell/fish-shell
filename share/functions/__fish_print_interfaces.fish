function __fish_print_interfaces --description "Print a list of known network interfaces"
    if test -d /sys/class/net
        set -l interfaces /sys/class/net/*
        string replace /sys/class/net/ '' $interfaces
    else # OSX/BSD
        set -l os (uname)
        if string match -e -q "BSD" -- $os
            command ifconfig | string match -e -r '^[a-z]' | string replace -r ':.*' '' | string split ' '
        else
            command ifconfig -l | string split ' '
        end
    end
end
