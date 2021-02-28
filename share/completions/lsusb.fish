function __fish_complete_lsusb
    lsusb | awk -F[ :] '{ print $2 ":" $4 }'
end

complete -c lsusb -s v -l verbose -d "Increase verbosity (show descriptors)"
complete -x -c lsusb -s s -a '(__fish_complete_lsusb)' -d "Show only devices with specified device and/or bus numbers (in decimal)"
complete -c lsusb -s d -d "Show only devices with the given vendor and product ID numbers (in hexadecimal)"
complete -c lsusb -s D -l device -d "Selects which device lsusb will examine"
complete -c lsusb -s t -l tree -d "Dump the physical USB device hierarchy as a tree"
complete -c lsusb -s V -l version -d "Show version of program"
complete -c lsusb -s h -l help -d "Show usage and help"
