function __fish_print_VBox_vms
    set -l print_names true
    set -l print_uuids true

    if set -q argv[1]
        switch $argv[1]
            case 'nouuids'
                set print_uuids false
            case 'nonames'
                set print_names false
        end
    end

    set -l lines (VBoxManage list vms | string match -r '"(.*)" {(.*)}')
    while set -q lines[2]
        if test $print_names = true
            printf '%s\tVirtual machine\n' $lines[2]
        end

        if test $print_uuids = true
            printf '%s\t%s virtual machine\n' $lines[3] $lines[2]
        end

        set -e lines[1]
        set -e lines[1]
        set -e lines[1]
    end
end
