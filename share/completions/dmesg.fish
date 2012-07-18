set -l levels '( __fish_complete_list , "echo emerg\nalert\ncrit\nerr\nwarn\nnotice\ninfo\ndebug" )'
complete -c dmesg -s C -l clear               -d 'Clear the kernel ring buffer'
complete -c dmesg -s c -l read-clear          -d 'Read and clear all messages'
complete -c dmesg -s D -l console-off         -d 'Disable printing messages to console'
complete -c dmesg -s d -l show-delta          -d 'Show time delta between printed messages'
complete -c dmesg -s E -l console-on          -d 'Enable printing messages to console'
complete -c dmesg -s F -l file                -d 'Use the file instead of the kernel log buffer'
complete -c dmesg -s f -l facility            -d 'Restrict output to defined facilities' -xa '( __fish_complete_list , "echo kern\nuser\nmail\ndaemon\nauth\nsyslog\nlpr\nnews" )'
complete -c dmesg -s h -l help                -d 'Display this help and exit'
complete -c dmesg -s k -l kernel              -d 'Display kernel messages'
complete -c dmesg -s l -l level               -d 'Restrict output to defined levels' -xa $levels
complete -c dmesg -s n -l console-level       -d 'Set level of messages printed to console' -xa $levels
complete -c dmesg -s r -l raw                 -d 'Print the raw message buffer'
complete -c dmesg -s s -l buffer-size         -d 'Buffer size to query the kernel ring buffer' -x
complete -c dmesg -s T -l ctime               -d 'Show human readable timestamp '
complete -c dmesg -s t -l notime              -d 'Don\'t print messages timestamp'
complete -c dmesg -s u -l userspace           -d 'Display userspace messages'
complete -c dmesg -s V -l version             -d 'Output version information and exit'
complete -c dmesg -s x -l decode              -d 'Decode facility and level to readable string'

