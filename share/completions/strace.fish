# Startup
complete -c strace -s E -l env -d 'Remove var from the inherited environment before passing it on to the command'
complete -c strace -s p -l attach -xa '(__fish_complete_pids)'
complete -c strace -s u -l user -xa '(__fish_complete_users)'
complete -c strace -l argv0 -d 'Set argv[0] of the command being executed to name'

# Tracing
complete -c strace -s b -l detach-on -d 'Detach when the specified syscall is reached'
complete -c strace -s D -l daemonize -d 'Run tracer process as a grandchild, not as the parent of the tracee'
complete -c strace -o DD -d "Run tracer process as tracee's grandchild in a separate process group."
complete -c strace -o DDD -d "Run tracer process as tracee's grandchild in a separate session"
complete -c strace -s f -l follow-forks
complete -c strace -l output-separately
complete -c strace -o ff -d 'Like --follow-forks --output-separately'
complete -c strace -s I -l interruptible -d 'When strace can be interrupted by signals'
complete -c strace -l syscall-limit -d 'Detach all tracees when LIMIT number of syscalls have been captured'
complete -c strace -l kill-on-exit -d 'Set PTRACE_O_EXITKILL ptrace option to all tracee processes'

# General/Filtering/Tampering
complete -c strace -s e -l trace -d 'Set expression which modifies which events to trace'
complete -c strace -s e -l trace-fds -d 'Set expression which modifies which events to trace'
complete -c strace -s e -l signal -d 'Set expression which modifies which events to trace'
complete -c strace -s e -l status -d 'Set expression which modifies which events to trace'
complete -c strace -s P -l trace-path -d 'Trace only system calls accessing path'
complete -c strace -s z -l successful-only -d 'Print only syscalls that returned without an error code'
complete -c strace -s Z -l failed-only -d 'Print only syscalls that returned with an error code'

# Output format
complete -c strace -s a -l columns -d 'Align output in columns'
complete -c strace -s e -l verbose -d 'Dereference structures for the specified set of system calls'
complete -c strace -l decode-pids -d 'Decode various information associated with process IDs'
complete -c strace -l silence -l silent -l quiet -d 'Suppress various information messages'
complete -c strace -l decode-fds -d 'Decode various information associated with file descriptors'
complete -c strace -l decode-pids -d 'Decode various information associated with process IDs'
complete -c strace -l kvm -d 'Print the exit reason of kvm vcpu'
complete -c strace -s i -l instruction-pointer -d 'Print the instruction pointer at the time of the system call'
complete -c strace -s n -l syscall-number -d 'Print the syscall number'
complete -c strace -s k -l stack-traces -d 'Print the execution stack trace of the traced processes after each system call'
complete -c strace -s o -l output -d 'Write the trace output to the file filename rather than to stderr'
complete -c strace -s A -l output-append-mode -d 'Open the file provided in the -o option in append mode'
complete -c strace -s q -l quiet
complete -c strace -o qq -l quiet
complete -c strace -o qqq -l quiet
complete -c strace -s r -l relative-timestamps -d 'Print a relative timestamp upon entry to each system call'
complete -c strace -s s -l string-limit -d 'Specify the maximum string size to print (the default is 32)'
complete -c strace -l absolute-timestamps -l timestamps -d 'Prefix each line of the trace with the wall clock time'
complete -c strace -s t -l absolute-timestamps -d 'Prefix each line of the trace with the wall clock time'
complete -c strace -o tt -l absolute-timestamps -d 'If given twice, the time printed will include the microseconds'
complete -c strace -o ttt -l absolute-timestamps
complete -c strace -s T -l syscall-times -d 'Show the time spent in system calls'
complete -c strace -s v -l no-abbrev -d 'Print unabbreviated versions of environment'
complete -c strace -l strings-in-hex -d 'Control usage of escape sequences with hexadecimal numbers in the printed strings'
complete -c strace -s x -d 'Print all non-ASCII strings in hex format'
complete -c strace -o xx -d 'Print all strings in hex format'
complete -c strace -s X -l const-print-style -d 'Set the format for printing of named constants and flags'
complete -c strace -o yy -l decode-fds -d 'Print all available information associated with file descriptors'
complete -c strace -s y -l decode-fds -d 'Print paths associated with file descriptor arguments and with the AT_FDCWD constant'
complete -c strace -l pidns-translation -l decode-pids -d "print PIDs in strace's namespace"
complete -c strace -s Y -l decode-pids -d 'Print command names for PIDs'

# Statistics
complete -c strace -s c -l summary-only -d 'Suppress output and report summary on exit'
complete -c strace -s C -l summary -d 'Display output and report summary on exit'
complete -c strace -s O -l summary-syscall-overhead -d 'Set the overhead for tracing system calls to overhead'
complete -c strace -s S -l summary-sort-by -xa 'time time_total total_time calls count errors error name syscall syscall_name nothing none' -d 'Sort summary by the specified criterion'
complete -c strace -s U -l summary-columns -d 'Configure a set (and order) of columns being shown in the call summary'
complete -c strace -s w -l summary-wall-clock -d 'Summarise the time difference between the beginning and end of each system call'

# Miscellaneous
complete -c strace -s d -l debug -d 'Debug output'
complete -c strace -l seccomp-bpf -d 'Try to enable use of seccomp-bpf'
complete -c strace -l tips -d 'Show strace tips, tricks, and tweaks before exit'
complete -c strace -s h -l help -d 'Print help and exit'
complete -c strace -s V -l version -d 'Print version and exit'
