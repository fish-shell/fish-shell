function __fish_complete_lsusb
    lsusb | awk -F[ :] '{ print $2 ":" $4 }'
end
