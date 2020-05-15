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
    set -l token (commandline -t | string split -f1 =)
    echo $token=(command journalctl -F $token 2>/dev/null)
end

complete -c journalctl -n "not __fish_journalctl_is_field" -a '(__fish_journalctl_fields)' -d "Journal field" -f
complete -c journalctl -n "not __fish_journalctl_is_field" -a '(command journalctl -F _EXE 2>/dev/null)' -d Executable
complete -c journalctl -n "not __fish_journalctl_is_field" -a '+' -d OR
complete -c journalctl -n __fish_journalctl_is_field -a '(__fish_journalctl_field_values)' -f -r

complete -c journalctl -f -s h -l help -d 'Prints a short help text and exits'
complete -c journalctl -f -l version -d 'Prints a short version string and exits'
complete -c journalctl -f -l no-pager -d 'Do not pipe output into a pager'
complete -c journalctl -f -s a -l all -d 'Show all fields in full'
complete -c journalctl -f -s f -l follow -d 'Show live tail of entries'
complete -c journalctl -f -s n -l lines -d 'Controls the number of journal lines'
complete -c journalctl -f -l no-tail -d 'Show all lines, even in follow mode'
complete -c journalctl -f -s o -l output -d 'Controls the formatting' -xa '(printf %s\t\n (command journalctl --output=help))'
complete -c journalctl -f -s q -l quiet -d 'Suppress warning about normal user'
complete -c journalctl -f -s m -l merge -d 'Show entries interleaved from all journals'

# Boot number (0 for current, -X for the Xst to last boot) with time as description
complete -c journalctl -f -l this-boot -d 'Show data only from the current boot'
complete -c journalctl -f -s b -l boot -d 'Show data only from a certain boot' -xa '(command journalctl 2>/dev/null --list-boots --no-pager | while read -l a b c; echo -e "$a\\t$c"; end)'
complete -c journalctl -f -s u -l unit -d 'Show data only of the specified unit' -xa "(__fish_systemctl_services)"
complete -c journalctl -f -s p -l priority -d 'Filter by priority' -xa 'emerg 0 alert 1 crit 2 err 3 warning 4 notice 5 info 6 debug 7'
complete -c journalctl -f -s c -l cursor -d 'Start from the passing cursor'
complete -c journalctl -f -l since -d 'Entries on or newer than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f -l until -d 'Entries on or older than DATE' -xa 'yesterday today tomorrow now'
complete -c journalctl -f -s F -l field -d 'Print all possible data values of FIELD' -xa '(printf %s\t\n (command journalctl --fields))'
complete -c journalctl -f -s D -l directory -d 'Specify journal directory' -xa "(__fish_complete_directories)"
complete -c journalctl -f -l new-id128 -d 'Generate a new 128 bit ID'
complete -c journalctl -f -l header -d 'Show internal header information'
complete -c journalctl -f -l disk-usage -d 'Shows the current disk usage'
complete -c journalctl -f -l setup-keys -d 'Generate Forward Secure Sealing key pair'
complete -c journalctl -f -l interval -d 'Change interval for the sealing'
complete -c journalctl -f -l verify -d 'Check journal for internal consistency'
complete -c journalctl -f -l verify-key -d 'Specifies FSS key for --verify'
complete -c journalctl -f -s r -l reverse -d "Reverse output to show newest entries first"
complete -c journalctl -f -l utc -d "Express time in Coordinated Universal Time (UTC)"
complete -c journalctl -f -l no-hostname -d "Don't show the hostname field"
complete -c journalctl -f -s x -l catalog -d "Augment log lines with explanation texts from the message catalog"
complete -c journalctl -f -l list-boots -d "Show a list of boot numbers, their IDs and timestamps"
complete -c journalctl -f -s k -l dmesg -d "Show only kernel messages"
complete -c journalctl -f -s N -l fields -d "Print all field names used in all entries of the journal"
complete -c journalctl -f -l update-catalog -d "Update the message catalog index"
complete -c journalctl -f -l sync -d "Write all unwritten journal data and sync journals"
complete -c journalctl -f -l flush -d "Flush log data from /run/log/journal/ into /var/log/journal/"
complete -c journalctl -f -l relinquish-var -d "Write to /run/log/journal/ instead of /var/log/journal/"
complete -c journalctl -f -l smart-relinquish-var -d "Similar to --relinquish-var"
complete -c journalctl -f -l rotate -d "Mark active journal files as archived and create new empty ones"
complete -c journalctl -f -l output-fields -d "List of fields to be included in the output"
complete -c journalctl -f -s t -l identifier -d "Show messages for specified syslog identifier" -xa '(printf %s\t\n (command journalctl -F SYSLOG_IDENTIFIER))'
complete -c journalctl -f -l user-unit -d "Show messages for the specified user session unit" -xa '(printf %s\t\n (command journalctl -F _SYSTEMD_USER_UNIT))'
complete -c journalctl -f -l facility -d "Filter output by syslog facility"
complete -c journalctl -f -s g -l grep -d "Show entries where MESSAGE field matches regex"
complete -c journalctl -f -l case-sensitive -d "Toggle pattern matching case sensitivity"
complete -c journalctl -F -l cursor-file -d "Load cursor from file or save to file, if missing"
complete -c journalctl -f -l after-cursor -d "Show entries after the passed cursor"
complete -c journalctl -f -l show-cursor -d "Show cursor after the last entry"
complete -c journalctl -f -l user -d "Show messages from service of current user"
complete -c journalctl -f -l system -d "Show messages from system services and the kernel"
complete -c journalctl -f -s M -l machine -d "Show messages from a running, local container"
complete -c journalctl -f -l file -d "Operate only on journal files matching glob"
complete -c journalctl -f -l root -d "Use specified directory instead of the root directory" -xa "(__fish_complete_directories)"
complete -c journalctl -f -l namespace -d "Show log data of specified namespace"
complete -c journalctl -f -l vacuum-size -d "Reduce disk usage below specified SIZE"
complete -c journalctl -f -l vacuum-time -d "Remove journal files older than TIME"
complete -c journalctl -f -l vacuum-files -d "Leave only INT number of journal files"
complete -c journalctl -f -l list-catalog -d "Show message catalog entries as a table"
complete -c journalctl -f -l dump-catalog -d "Show message catalog entries"
