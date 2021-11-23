function __fish_systemd_units
    systemctl list-unit-files --full --no-legend --no-pager --plain 2>/dev/null | string split -f 1 ' '
    systemctl list-units --state=loaded --full --no-legend --no-pager --plain 2>/dev/null | string split -f 1 ' '
end

# global options

complete -c systemd-analyze -f
complete -c systemd-analyze -l system -d 'Operates on the system systemd instance' -n "not __fish_contains_opt system user global"
complete -c systemd-analyze -l user -d 'Operates on the user systemd instance' -n "not __fish_contains_opt system user global"
complete -c systemd-analyze -l global -d 'Operates on the system-wide configuration for user systemd instance' -n "not __fish_contains_opt system user global"
complete -c systemd-analyze -l order -d 'dot: show only After and Before dependencies' -n "not __fish_contains_opt order require"
complete -c systemd-analyze -l require -d 'dot: show only Requires, Requisite, Wants and Conflicts dependencies' -n "not __fish_contains_opt order require"
complete -c systemd-analyze -l from-pattern -d 'dot: show relationships matching left-hand nodes pattern'
complete -c systemd-analyze -l to-pattern -d 'dot: show relationships matching right-hand nodes pattern'
complete -c systemd-analyze -l fuzz -x -d 'critical-chain: also show units, which finished timespan earlier, than the latest unit in the same level'
complete -c systemd-analyze -l man -xa no -d 'Do not invoke man to verify the existence of man pages'
complete -c systemd-analyze -l generators -d 'Invoke unit generators'
complete -c systemd-analyze -l root -xa "(__fish_complete_directories)" -d 'With cat-files, show config files underneath the specified root path'
complete -c systemd-analyze -l iterations -x -d 'calendar: show number of iterations the calendar expression will elapse next'
complete -c systemd-analyze -l base-time -x -d 'calendar: show next iterations relative to the specified point in time'
complete -c systemd-analyze -s H -l host -xa "(__fish_complete_user_at_hosts)" -d 'Execute the operation on a remote host'
complete -c systemd-analyze -s M -l machine -xa "(__fish_systemd_machines)" -d 'Execute operation on a VM or container'
complete -c systemd-analyze -s h -l help -d 'Print a short help and exit'
complete -c systemd-analyze -l version -d 'Print a short version and exit'
complete -c systemd-analyze -l no-pager -d 'Do not pipe output into a pager'

# subcommands

complete -c systemd-analyze -n __fish_use_subcommand -a time -d "Print timing statistics"
complete -c systemd-analyze -n __fish_use_subcommand -a blame -d "Print list of running units ordered by time to init"

complete -c systemd-analyze -n __fish_use_subcommand -a critical-chain -d "Print a tree of the time critical chain of units"
complete -c systemd-analyze -n "__fish_seen_subcommand_from critical-chain" -a "(__fish_systemd_units)"

complete -c systemd-analyze -n __fish_use_subcommand -a dump -d "Output serialization of server state"
complete -c systemd-analyze -n __fish_use_subcommand -a plot -d "Output SVG graphic showing service initialization"
complete -c systemd-analyze -n __fish_use_subcommand -a dot -d "Output dependency graph in dot(1) format"
complete -c systemd-analyze -n __fish_use_subcommand -a unit-paths -d "List all directories from which unit files may be loaded"
complete -c systemd-analyze -n __fish_use_subcommand -a exit-status -d "List exit statuses along with their class"
complete -c systemd-analyze -n __fish_use_subcommand -a capability -d "List Linux capabilities along with their numeric IDs"
complete -c systemd-analyze -n __fish_use_subcommand -a condition -d "Evaluate Condition and Assert assignments"
complete -c systemd-analyze -n __fish_use_subcommand -a syscall-filter -d "List system calls contained in the specified system call set"
complete -c systemd-analyze -n __fish_use_subcommand -a calendar -d "Normalize repetitive calendar events and calculate when they elapse next"
complete -c systemd-analyze -n __fish_use_subcommand -a timestamp -d "Parse timestamp and output the normalized form"
complete -c systemd-analyze -n __fish_use_subcommand -a timestamp -d "Parse time span and output the normalized form"

complete -c systemd-analyze -n __fish_use_subcommand -a cat-config -d "Show contents of a config file"
complete -c systemd-analyze -n "__fish_seen_subcommand_from cat-config" -F

complete -c systemd-analyze -n __fish_use_subcommand -a verify -d "Check unit files for correctness"
complete -c systemd-analyze -n "__fish_seen_subcommand_from verify" -F

complete -c systemd-analyze -n __fish_use_subcommand -a security -d "Analyze security settings of specified service units"
complete -c systemd-analyze -n "__fish_seen_subcommand_from security" -a "(__fish_systemctl_services)"
