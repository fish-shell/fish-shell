__fish_make_completion_signals
for i in $__kill_signals
    string split -f 1,2 " " -- $i | read --line number name
    complete -c fuser -o $number -d $name
    complete -c fuser -o $name -d $name
end

complete -c fuser -s a -l all -d 'Show all files specified on the command line'
complete -c fuser -s k -l kill -d 'Kill processes, accessing the file'
complete -c fuser -s i -d 'Ask the user for confirmation before killing a process'
complete -c fuser -s l -l list-signals -d 'List all known signal names'
complete -c fuser -s m -l mount -d 'All processes accessing files on that file system are listed' -xa '(__fish_print_mounted)'
complete -c fuser -s M -l ismountpoint -d 'Request will be fulfilled if -m specifies a mountpoint'
complete -c fuser -s w -d 'Kill only processes which have write access'
complete -c fuser -s n -l namespace -d 'Slect a different namespace' -r
complete -c fuser -s s -l silent -d 'Silent operation'
complete -c fuser -s u -l user -d 'Append the user name of the process owner to each PID'
complete -c fuser -s v -l verbose -d 'Verbose mode'
complete -c fuser -s V -d 'Print version and exit'
complete -c fuser -s 4 -l ipv4 -d 'Search only for IPv4 sockets'
complete -c fuser -s 6 -l ip64 -d 'Search only for IPv6 sockets'
