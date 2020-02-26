# Startup
complete -c strace -s p -l attach -xa '(__fish_complete_pids)'
complete -c strace -s u -l user -xa '(__fish_complete_users)'

# Tracing
complete -c strace -s b -l detach-on -d 'Detach when the specified syscall is reached'

# General/Filtering/Tampering
complete -c strace -s e -d 'Set expression which modifies which events to trace'

# Output format
complete -c strace -s a -l columns -d 'Align output in columns'
complete -c strace -s x -d 'Print all non-ASCII strings in hex format'
complete -c strace -s xx -d 'Print all strings in hex format'

# Statistics
complete -c strace -s c -l summary-only -d 'Suppress output and report summary on exit'
complete -c strace -s C -l summary -d 'Display output and report summary on exit'
complete -c strace -s S -l summary-sort-by -xa 'time time_total total_time calls count errors error name syscall syscall_name nothing none' -d 'Sort summary by the specified criterion'

# Miscellaneous
complete -c strace -s d -l debug -d 'Debug output'
complete -c strace -s h -l help -d 'Print help and exit'
complete -c strace -s V -l version -d 'Print version and exit'
