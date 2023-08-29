complete -c dmesg -f -d 'Display system message buffer'

switch (uname -s)
    #
    # Solaris
    #
    case SunOS
        exit 1 # no options, we are done
        #
        # Loonix dmesg
        #
    case Linux
        set -l levels '( __fish_append , emerg alert crit err warn notice info debug)'
        complete -c dmesg -s C -l clear -f -d'Clear kernel ring buffer'
        complete -c dmesg -s c -l read-clear -f -d'Read & clear all msgs'
        complete -c dmesg -s D -l console-off -f -d'Disable writing to console'
        complete -c dmesg -s d -l show-delta -f -d'Show timestamp deltas'
        complete -c dmesg -s E -l console-on -f -d'Enable writing to console'
        complete -c dmesg -s F -l file -r -d'Use file instead of log buffer'
        complete -c dmesg -s f -l facility -x -d'Only print for given facilities' -a '( __fish_append , kern user mail daemon auth syslog lpr news)'
        complete -c dmesg -s h -l help -f -d'Display help'
        complete -c dmesg -s k -l kernel -f -d'Print kernel messages'
        complete -c dmesg -s l -l level -x -d'Restrict output to given levels' -a $levels
        complete -c dmesg -s n -l console-level -x -d'Adjust threshold to print to console' -a $levels
        complete -c dmesg -s r -l raw -f -d'Print raw message buffer'
        complete -c dmesg -s s -l buffer-size -x -d'Buffer size to query kernel'
        complete -c dmesg -s T -l ctime -f -d'Human-readable timestamps'
        complete -c dmesg -s t -l notime -f -d'Don\'t print timestamps'
        complete -c dmesg -s u -l userspace -f -d'Print userspace messages'
        complete -c dmesg -s V -l version -f -d'Show dmesg version'
        complete -c dmesg -s w -l follow -f -d'Wait for new messages'
        complete -c dmesg -s W -l follow-new -f -d'Wait and print only new messages'
        complete -c dmesg -s x -l decode -f -d'Decode facility & level numbers'
        exit 0 # done
        #
        # unique options specific BSDs have
        #
    case NetBSD
        complete -c dmesg -s d -f -d'show timestamp deltas'
        complete -c dmesg -s T -f -d'human-readable timestamps'
        complete -c dmesg -s t -f -d'don\'t print timestamps'
    case OpenBSD
        complete -c dmesg -s S -f -d'display console message buffer-size'
    case FreeBSD DragonFly
        complete -c dmesg -s a -f -d'print all data in the message buffer'
        complete -c dmesg -s c -f -d'clear kernel buffer after printing'
end

#
# common BSD dmesg options (macOS only does these two)
#
complete -c dmesg -s M -r -d'get namelist values from given core'
complete -c dmesg -s N -r -d'get namelist from given kernel'
