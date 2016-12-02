function __fish_print_VBox_vms
    set -l print_names true
    set -l print_uuids true

    if contains -- nouuids $argv
        set print_uuids false
    end

    if contains -- nonames $argv
        set print_names false
    end

    # `VBoxManage list vms` output example:
    # "Machine Name" {222537b0-1ea1-454a-abf0-ed0d6c3c6346}
    # "Another Machine" {332537b4-1ea1-454a-abf0-ed1d6c3c2347}
    set -l lines (VBoxManage list vms | string match -r '"(.*)" {(.*)}')

    while set -q lines[3]
        if test $print_names = true
            printf '%s\tVirtual machine\n' $lines[2]
        end

        if test $print_uuids = true
            printf '%s\t%s virtual machine\n' $lines[3] $lines[2]
        end

        set -e lines[1..3]
    end
end
