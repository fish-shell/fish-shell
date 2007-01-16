
complete -c ulimit -s S -l soft --description "Set or get soft limit"
complete -c ulimit -s H -l hard --description "Set or get hard limit"

complete -c ulimit -s a -l all --description "Set or get all current limits"
complete -c ulimit -s c -l core-size --description "Maximum size of core files created"
complete -c ulimit -s d -l data-size --description "Maximum size of a process's data segment"
complete -c ulimit -s f -l file-size --description "Maximum size of files created by the shell"
complete -c ulimit -s l -l lock-size --description "Maximum size that may be locked into memory"
complete -c ulimit -s m -l resident-set-size --description "Maximum resident set size"
complete -c ulimit -s n -l file-descriptor-count --description "Maximum number of open file descriptors"
complete -c ulimit -s s -l stack-size --description "Maximum stack size"
complete -c ulimit -s t -l cpu-time --description "Maximum amount of cpu time in seconds"
complete -c ulimit -s u -l process-count --description "Maximum number of processes available to a single user"
complete -c ulimit -s v -l virtual-memory-size --description "Maximum amount of virtual memory available to the shell"

complete -c ulimit -s h -l help --description "Display help and exit"

complete -c ulimit -a "unlimited soft hard" --description "New resource limit"
