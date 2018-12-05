
complete -c ulimit -s S -l soft -d "Set or get soft limit"
complete -c ulimit -s H -l hard -d "Set or get hard limit"

complete -c ulimit -s a -l all -d "Get all current limits"
complete -c ulimit -s c -l core-size -d "Maximum size of core files created"
complete -c ulimit -s d -l data-size -d "Maximum size of a process's data segment"
complete -c ulimit -s f -l file-size -d "Maximum size of files created by the shell"
complete -c ulimit -s l -l lock-size -d "Maximum size that may be locked into memory"
complete -c ulimit -s m -l resident-set-size -d "Maximum resident set size"
complete -c ulimit -s n -l file-descriptor-count -d "Maximum number of open file descriptors"
complete -c ulimit -s s -l stack-size -d "Maximum stack size"
complete -c ulimit -s t -l cpu-time -d "Maximum amount of cpu time in seconds"
complete -c ulimit -s u -l process-count -d "Maximum number of processes available to a single user"
complete -c ulimit -s v -l virtual-memory-size -d "Maximum amount of virtual memory available to the shell"

complete -c ulimit -s h -l help -d "Display help and exit"

complete -c ulimit -a "unlimited soft hard" -d "New resource limit"
