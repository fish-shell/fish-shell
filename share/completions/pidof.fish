complete -c pidof -s s -d "Single shot: return only one PID"
complete -c pidof -s c -d "Only return processes w/ same root directory"
complete -c pidof -s n -d "Avoid stat(2) for binaries on NFS shares"
complete -c pidof -s x -d "Include PIDs of shell scripts matching name"

# Dynamically complete all PIDs + special variable %PPID for parent process
complete -c pidof -s o -d "Exclude processes with PID" -xa '%PPID\t"Parent process, i.e. fish" (__fish_complete_pids)'

# Dynamically complete all running processes
complete -c pidof -xa '(__fish_complete_proc)'
