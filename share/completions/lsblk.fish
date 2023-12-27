function __fish_print_lsblk_columns --description 'Print available lsblk columns'
    LC_ALL=C lsblk --help | sed '1,/Available .*columns:/d; /^$/,$d; s/^[[:space:]]\+//; s/[[:space:]]/\t/'
end

complete -c lsblk -s a -l all -d "print all devices"
complete -c lsblk -s b -l bytes -d "print SIZE in bytes rather than in human readable format"
complete -c lsblk -s d -l nodeps -d "don't print slaves or holders"
complete -c lsblk -s D -l discard -d "print discard capabilities"
complete -c lsblk -s e -l exclude -d "exclude devices by major number (default: RAM disks)"
complete -c lsblk -s f -l fs -d "output info about filesystems"
complete -c lsblk -s h -l help -d "usage information (this)"
complete -c lsblk -s i -l ascii -d "use ascii characters only"
complete -c lsblk -s m -l perms -d "output info about permissions"
complete -c lsblk -s n -l noheadings -d "don't print headings"
complete -c lsblk -s o -l output -d "output columns" -xa '( __fish_complete_list , __fish_print_lsblk_columns )'
complete -c lsblk -s P -l pairs -d "use key='value' output format"
complete -c lsblk -s r -l raw -d "use raw output format"
complete -c lsblk -s t -l topology -d "output info about topology"
complete -c lsblk -s l -l list -d "use list format output"
