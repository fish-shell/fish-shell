function __fish_journalctl_fields
    # This would be nicer to get programatically, but this is how the official
    # bash completion does it
    set -l __journal_fields MESSAGE{,_ID} PRIORITY CODE_{FILE,LINE,FUNC} ERRNO SYSLOG_{FACILITY,IDENTIFIER,PID} COREDUMP_EXE _{P,U,G}ID _COMM _EXE _CMDLINE _AUDIT_{SESSION,LOGINUID} _SYSTEMD_{CGROUP,SESSION,UNIT,OWNER_UID} _SELINUX_CONTEXT _SOURCE_REALTIME_TIMESTAMP _{BOOT,MACHINE}_ID _HOSTNAME _TRANSPORT _KERNEL_{DEVICE,SUBSYSTEM} _UDEV_{SYSNAME,DEVNODE,DEVLINK} __CURSOR __{REALTIME,MONOTONIC}_TIMESTAMP
    for f in $__journal_fields
        echo $f"="
    end
end

function __fish_journalctl_is_field
    set -l token (commandline -t)
    if contains -- $token (__fish_journalctl_fields)
        return 0
    end
    return 1
end

function __fish_journalctl_field_values
    set -l token (commandline -t | cut -d"=" -f 1)
    command journalctl -F $token 2>/dev/null | while read value
        echo $token=$value
    end
end

complete -c journalctl -n "not __fish_journalctl_is_field" -a '(__fish_journalctl_fields)' -d "Journal field" -f
complete -c journalctl -n "not __fish_journalctl_is_field" -a '(command journalctl -F _EXE 2>/dev/null)' -d "Executable"
complete -c journalctl -n "not __fish_journalctl_is_field" -a '+' -d "OR"
complete -c journalctl -n "__fish_journalctl_is_field" -a '(__fish_journalctl_field_values)' -f -r

complete -c journalctl -f -s h -l help -d 'Prints a short help text and exits'
complete -c journalctl -f -l version -d 'Prints a short version string and exits'
complete -c journalctl -f -l no-pager -d 'Do not pipe output into a pager'
complete -c journalctl -f -s a -l all -d 'Show all fields in full'
complete -c journalctl -f -s f -l follow -d 'Show live tail of entries'
complete -c journalctl -f -s n -l lines -d 'Controls the number of journal lines'
complete -c journalctl -f -l no-tail -d 'Show all lines, even in follow mode'
complete -c journalctl -f -s o -l output -d 'Controls the formatting' -xa 'short short-monotonic verbose export json json-pretty json-sse cat'
complete -c journalctl -f -s q -l quiet -d 'Suppress warning about normal user'
complete -c journalctl -f -s m -l merge -d 'Show entries interleaved from all journals'

# Boot number (0 for current, -X for the Xst to last boot) with time as description
complete -c journalctl -f -s b -l this-boot -d 'Show data only from a certain boot' -a '(journalctl --list-boots --no-pager | while read a b c; echo -e "$a\\t$c"; end)'
complete -c journalctl -f -s u -l unit -d 'Show data only of the specified unit' -xa "(__fish_systemctl_services)"
complete -c journalctl -f -s p -l priority -d 'Filter by priority' -xa 'emerg 0 alert 1 crit 2 err 3 warning 4 notice 5 info 6 debug 7'
complete -c journalctl -f -s c -l cursor -d 'Start from the passing cursor'
complete -c journalctl -f -l since -d 'Entries on or newer than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f -l until -d 'Entries on or older than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f -s F -l field -d 'Print all possible data values'
complete -c journalctl -f -s D -l directory -d 'Specify journal directory' -xa "(__fish_complete_directories)"
complete -c journalctl -f -l new-id128 -d 'Generate a new 128 bit ID'
complete -c journalctl -f -l header -d 'Show internal header information'
complete -c journalctl -f -l disk-usage -d 'Shows the current disk usage'
complete -c journalctl -f -l setup-keys -d 'Generate Forward Secure Sealing key pair'
complete -c journalctl -f -l interval -d 'Change interval for the sealing'
complete -c journalctl -f -l verify -d 'Check journal for internal consistency'
complete -c journalctl -f -l verify-key -d 'Specifies FSS key for --verify'
