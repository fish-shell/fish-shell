# Completions for hwinfo

set -l hardware_items all arch bios block bluetooth braille bridge camera \
    cdrom chipcard cpu disk dsl dvb fingerprint floppy framebuffer gfxcard \
    hub ide isapnp isdn joystick keyboard memory mmc-ctrl modem monitor mouse \
    netcard network partition pci pcmcia pcmcia-ctrl pppoe printer redasd \
    reallyall scanner scsi smp sound storage-ctrl sys tape tv uml usb \
    usb-ctrl vbe wlan xen zip

for i in $hardware_items
    complete -f -c hwinfo -l $i -d "Probe the specified HARDWARE_ITEM"
end

complete -f -c hwinfo -l short -d "Show only a summary"
complete -f -c hwinfo -l listmd -d "Report RAID devices"
complete -f -c hwinfo -l only -d "Show only entries in the device list matching DEVNAME"
complete -f -c hwinfo -l save-config -d "Store config for a particular device"
complete -f -c hwinfo -l show-config -d "Show saved config data"
complete -f -c hwinfo -l map -d "Prints a list of disk name mappings"
complete -f -c hwinfo -l debug -d "Set debug level"
complete -f -c hwinfo -l verbose -d "Increase verbosity"
complete -f -c hwinfo -l log -d "Write log"
complete -f -c hwinfo -l dump-db -d "Dump hardware database"
complete -f -c hwinfo -l version -d "Show libhd version"
complete -f -c hwinfo -l help -d "Show help message"
